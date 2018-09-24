using Microsoft.Azure.Devices;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Azure.EventHubs;

namespace iotpnp_service
{
    public class PnpInterface
    {
        private readonly string ConnectionString;

        public PnpInterface(JObject json, string connectionString)
        {
            Id = json["id"].ToString();
            Events = JsonConvert.DeserializeObject<List<string>>(json["Events"].ToString());

            Properties = new List<string>();
            var props = (JObject)json["Properties"];
            foreach (var prop in props)
            {
                Properties.Add(prop.Key);
            }

            Commands = new List<string>();
            var cmds = (JObject)json["Commands"];
            foreach (var cmd in cmds)
            {
                Commands.Add(cmd.Key);
            }

            ConnectionString = connectionString;
        }

        private List<string> Events;
        private List<string> Properties;
        private List<string> Commands;

        public async Task<string> InvokeMethod(string cmdType, string cmdName, string input)
        {
            JObject json = new JObject(
                           new JProperty("interfaceid", Id),
                           new JProperty("command",
                               new JObject(
                                   new JProperty("type", cmdType),
                                   new JProperty("name", cmdName))),
                           new JProperty("input", input));

            var methodInvocation = new CloudToDeviceMethod("PnpCommandHandler" + Id) { ResponseTimeout = TimeSpan.FromSeconds(30) };
            methodInvocation.SetPayloadJson(json.ToString());

            var serviceClient = ServiceClient.CreateFromConnectionString(ConnectionString);

            // Invoke the direct method asynchronously and get the response from the simulated device.
            var response = await serviceClient.InvokeDeviceMethodAsync("win-gateway", methodInvocation);

            Console.WriteLine("Response status: {0}, payload:", response.Status);
            Console.WriteLine(response.GetPayloadAsJson());

            return response.GetPayloadAsJson();
        }

        public async Task<string> ReadProperty(string Name)
        {
            if (!Properties.Contains(Name))
            {
                throw new ArgumentException("Interface " + Id + " doesn't contain the property " + Name);
            }

            return await InvokeMethod("propread", Name, "");
        }

        public async Task WriteProperty(string Name, string value)
        {
            if (!Properties.Contains(Name))
            {
                throw new ArgumentException("Interface " + Id + " doesn't contain the property " + Name);
            }

            await InvokeMethod("propwrite", Name, value);
        }

        public async Task<string> SendCommand(string Name, string input)
        {
            if (!Commands.Contains(Name))
            {
                throw new ArgumentException("Interface " + Id + " doesn't contain the property " + Name);
            }

            return await InvokeMethod("cmd", Name, input);
        }

        public string Id { get; private set; }

        private async Task ReceiveMessagesFromDeviceAsync(string Name, EventHubClient eventHubClient, string partition, CancellationToken ct)
        {
            // Create the receiver using the default consumer group.
            // For the purposes of this sample, read only messages sent since 
            // the time the receiver is created. Typically, you don't want to skip any messages.
            var eventHubReceiver = eventHubClient.CreateReceiver("$Default", partition, EventPosition.FromEnqueuedTime(DateTime.Now));
            Console.WriteLine("Create receiver on partition: " + partition);
            Console.WriteLine("Listening for messages on: " + partition);
            while (true)
            {
                if (ct.IsCancellationRequested) break;
                // Check for EventData - this methods times out if there is nothing to retrieve.
                var events = await eventHubReceiver.ReceiveAsync(100);

                // If there is data in the batch, process it.
                if (events == null) continue;

                foreach (EventData eventData in events)
                {
                    if (!eventData.Properties.ContainsKey("interfaceid") || eventData.Properties["interfaceid"].ToString() != Id)
                    {
                        continue;
                    }
                    if (eventData.Properties["eventname"].ToString() == Name)
                    {
                        string data = Encoding.UTF8.GetString(eventData.Body.Array);
                        Console.WriteLine("Value: " + data);
                    }
                }
            }
        }
        // HostName=iot-pnp-hub1.azure-devices.net;SharedAccessKeyName=iothubowner;SharedAccessKey=buS4T5II3EDIPfXd/biEBiDIOpnRbyUJcOcbsuIVHpk=
        // Event Hub-compatible endpoint
        // az iot hub show --query properties.eventHubEndpoints.events.endpoint --name {your IoT Hub name}
        private readonly static string s_eventHubsCompatibleEndpoint = "sb://iothub-ns-iot-pnp-hu-794875-26efe2bd56.servicebus.windows.net/;SharedAccessKeyName=iothubowner;SharedAccessKey=buS4T5II3EDIPfXd/biEBiDIOpnRbyUJcOcbsuIVHpk=;";

        // Event Hub-compatible name
        // az iot hub show --query properties.eventHubEndpoints.events.path --name {your IoT Hub name}
        private readonly static string s_eventHubsCompatiblePath = "iot-pnp-hub1";

        // az iot hub policy show --name iothubowner --query primaryKey --hub-name {your IoT Hub name}
        private readonly static string s_iotHubSasKey = "buS4T5II3EDIPfXd/biEBiDIOpnRbyUJcOcbsuIVHpk=";
        private readonly static string s_iotHubSasKeyName = "iothubowner";

        public async Task ListenToEvent(string Name, CancellationTokenSource cts)
        {
            if (!Events.Contains(Name))
            {
                throw new ArgumentException("Interface " + Id + " doesn't contain the event " + Name);
            }

            EventHubClient eventHubClient;

            var connectionString = new EventHubsConnectionStringBuilder(new Uri(s_eventHubsCompatibleEndpoint), s_eventHubsCompatiblePath, s_iotHubSasKeyName, s_iotHubSasKey);
            eventHubClient = EventHubClient.CreateFromConnectionString(connectionString.ToString());

            // Create a PartitionReciever for each partition on the hub.
            var runtimeInfo = await eventHubClient.GetRuntimeInformationAsync();
            var d2cPartitions = runtimeInfo.PartitionIds;

            var tasks = new List<Task>();
            foreach (string partition in d2cPartitions)
            {
                tasks.Add(ReceiveMessagesFromDeviceAsync(Name, eventHubClient, partition, cts.Token));
            }
        }
    }

}
