#include "azure_umqtt_c/mqtt_client.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/const_defines.h"

#include "parson.h"

#include <stdexcept>
#include <map>
#include <atomic>
#include <thread>

#include <pnpbridge.h>

#include "mqtt_protocol_handler.hpp"
#include "mqtt_manager.hpp"

void
MqttConnectionManager::Subscribe(
    const char*         Topic,
    MqttProtocolHandler *ProtocolHandler
)
{
    SUBSCRIBE_PAYLOAD subscribe[2];
    subscribe[0].subscribeTopic = Topic;
    subscribe[0].qosReturn = DELIVER_AT_MOST_ONCE;

    printf("MQTT subscribing to topic %s\n", Topic);

    if (mqtt_client_subscribe(s_MqttClientHandle, GetNextPacketId(), subscribe, 1) != 0) {
        printf("MQTT subscribe failed\n");
        Disconnect();
        throw std::invalid_argument("Problem subscribing to MQTT channel");
    }

    // Add to mapping
    s_Topics.insert(std::pair<std::string, MqttProtocolHandler*>(std::string(Topic), ProtocolHandler));
}

uint16_t
MqttConnectionManager::GetNextPacketId()
{
    // Skip over id = 0
    if (++s_NextPacketId == 0) {
        s_NextPacketId ++;
    }

    return s_NextPacketId;
}

void
MqttConnectionManager::Publish(
    const char*         Topic,
    const char*         Message,
    size_t              MessageSize
)
{
    MQTT_MESSAGE_HANDLE msg = mqttmessage_create(GetNextPacketId(),
                                                 Topic,
                                                 DELIVER_EXACTLY_ONCE,
                                                 (const uint8_t*) Message,
                                                 MessageSize);
    if (msg == nullptr) {
        throw std::runtime_error("Couldn't allocate MQTT publish message");
    }

     if (mqtt_client_publish(s_MqttClientHandle, msg)) {
        mqttmessage_destroy(msg);
        Disconnect();
        throw std::invalid_argument("Error publishing MQTT message");
    } else {
        mqttmessage_destroy(msg);
    }

}

void
MqttConnectionManager::Connect(
    const char*         Server,
    int                 Port
)
{
    MQTT_CLIENT_OPTIONS mqtt_options = { 0 };
    mqtt_options.clientId = "azureiotclient";
    mqtt_options.willMessage = NULL;
    mqtt_options.username = NULL;
    mqtt_options.password = NULL;
    mqtt_options.keepAliveInterval = 10;
    mqtt_options.useCleanSession = true;
    mqtt_options.qualityOfServiceValue = DELIVER_AT_MOST_ONCE;
    // todo: port, qos
    SOCKETIO_CONFIG socket_config = { Server, Port, NULL };

    s_MqttClientHandle =
        mqtt_client_init(
            // OnRecv Callback
            [](
                MQTT_MESSAGE_HANDLE         MessageHandle,
                void*                       Context
            )
            {
                static_cast<MqttConnectionManager*>(Context)->OnRecv(MessageHandle);
            },
            // OnComplete Callback
            [](
                MQTT_CLIENT_HANDLE          ClientHandle,
                MQTT_CLIENT_EVENT_RESULT    Result, 
                const void*                 MessageInfo,
                void*                       Context
            )
            {
                static_cast<MqttConnectionManager*>(Context)->OnComplete(ClientHandle, Result, MessageInfo);
            },
            (void*) this,
            // OnError Callback
            [](
                MQTT_CLIENT_HANDLE          ClientHandle,
                MQTT_CLIENT_EVENT_ERROR     Error,
                void*                       Context
            )
            {
                static_cast<MqttConnectionManager*>(Context)->OnError(ClientHandle, Error);
            },
            (void*) this
        );

    if (!s_MqttClientHandle) {
        throw std::runtime_error("Couldn't allocate new mqtt client handle");
    }

    s_XioHandle = xio_create(socketio_get_interface_description(), &socket_config);
    if (!s_XioHandle) {
        mqtt_client_deinit(s_MqttClientHandle);
        s_MqttClientHandle = nullptr;
        throw std::runtime_error("Couldn't create xio handle");
    }

    printf("mqtt: created socket\n");

    if (mqtt_client_connect(s_MqttClientHandle, s_XioHandle, &mqtt_options) != 0) {
        mqtt_client_deinit(s_MqttClientHandle);
        s_MqttClientHandle = nullptr;

        auto xh = s_XioHandle;
        s_XioHandle = nullptr;
        xio_close(xh, nullptr, nullptr);

        throw std::invalid_argument("Could not connect to MQTT");
    }

    printf("mqtt: created socket 2\n");

    // Wait for successful connection
    s_ProcessOperation = true;
    while (s_ProcessOperation) {
        mqtt_client_dowork(s_MqttClientHandle);
    }

    printf("mqtt: processed connect\n");
    
    if (!s_OperationSuccess) {
        Disconnect();
        throw std::invalid_argument("Problem getting connect ACK from MQTT");
    }

    printf("mqtt: starting worker\n");

    // Start worker thread
    s_RunWorker = true;
    s_WorkerThread = std::thread([&](){
        while (s_RunWorker) {
            mqtt_client_dowork(s_MqttClientHandle);
        }
    });
    // s_WorkerThread.detach();
}

void
MqttConnectionManager::OnRecv(
    MQTT_MESSAGE_HANDLE         MessageHandle
)
{
    const char* topicName = mqttmessage_getTopicName(MessageHandle);
    MqttProtocolHandler* handler_for_topic = nullptr;
    const APP_PAYLOAD* mqtt_msg = mqttmessage_getApplicationMsg(MessageHandle);

    auto topic_str = std::string(topicName);

    auto iterator = s_Topics.find(topic_str);
    if (iterator != s_Topics.end()) {
        handler_for_topic = iterator->second;
    }

    printf("MQTT recv on topic %s, value %s\n", topicName, mqtt_msg->message);

    if (handler_for_topic) {
        handler_for_topic->OnReceive(topicName,
                                     (const char*) mqtt_msg->message,
                                     mqtt_msg->length);
    }
}

void
MqttConnectionManager::OnComplete(
    MQTT_CLIENT_HANDLE          /*ClientHandle*/,
    MQTT_CLIENT_EVENT_RESULT    Result, 
    const void*                 /*MessageInfo*/
)
{
    printf("MQTT complete callback\n");
    switch (Result) {
    case MQTT_CLIENT_ON_CONNACK:
        printf("MQTT CONNACK\n");
        s_OperationSuccess = true;
        s_ProcessOperation = false;
        break;
    case MQTT_CLIENT_ON_DISCONNECT:
        printf("MQTT DISCONNECT\n");
        // todo stop read thread here
        break;
    default:
        printf("OTHER MQTT %d\n", Result);
        break;
    }
}

void
MqttConnectionManager::OnError(
    MQTT_CLIENT_HANDLE          /*ClientHandle*/,
    MQTT_CLIENT_EVENT_ERROR     /*Error*/
)
{
    printf("MQTT error callback\n");
    s_OperationSuccess = false;
    s_ProcessOperation = false;
}

void
MqttConnectionManager::Disconnect()
{
    printf("MQTT DISCONNECT REQUEST\n");
    bool workerRunning = s_RunWorker;
    if (s_RunWorker) {
        s_RunWorker = false;
    }
    mqtt_client_disconnect(s_MqttClientHandle,
                           [](void* Context){
                                auto mcm = static_cast<MqttConnectionManager*>(Context);
                                xio_close(mcm->s_XioHandle,
                                          [](void* /*Context*/){
                                                // auto mcm = static_cast<MqttConnectionManager*>(Context);
                                                // delete mcm;
                                            },
                                          Context);
                            },
                            this);

    if (workerRunning) {
        s_WorkerThread.join();
    }
}