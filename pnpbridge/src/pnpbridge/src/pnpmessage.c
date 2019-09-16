#include "pnpbridge_common.h"

// Custom destroy method for PNPMESSAGE's PNPMEMORY
void 
PnpMessage_Destroy(
    _In_ PNPMEMORY Message
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);

    if (msg->Message) {
        free((void*)msg->Message);
    }

    if (msg->InterfaceId) {
        free((void*)msg->InterfaceId);
    }

    if (msg->Properties.ComponentName) {
        free(msg->Properties.ComponentName);
    }
}

const char* 
PnpMessage_GetMessageBuffer(
    _In_ PNPMESSAGE Message,
    _Out_opt_ int *Length
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg;
    
    msg = (PPNPBRIDGE_CHANGE_PAYLOAD) PnpMemory_GetBuffer(Message, NULL);

    if (Length) {
        *Length = msg->MessageLength;
    }
    return msg->Message;
}

int 
PnpMessage_CreateMessage(
    _In_ PNPMESSAGE* Message
    )
{
    PNPMEMORY_ATTRIBUTES attrib = { 0 };
    PNPMESSAGE* message = NULL;
    attrib.destroyCallback = PnpMessage_Destroy;

    PnpMemory_Create(&attrib, sizeof(PNPBRIDGE_CHANGE_PAYLOAD), (PNPMEMORY*)&message);

    PPNPBRIDGE_CHANGE_PAYLOAD msg = (PPNPBRIDGE_CHANGE_PAYLOAD) PnpMemory_GetBuffer(message, NULL);
    /* msg->MessageLength = MessageSize;
    msg->Message = (char*) (msg + sizeof(PNPBRIDGE_DEVICE_CHANGE_PAYLOAD));*/
    DList_InitializeListHead(&msg->Entry);
    msg->Self = message;

    *Message = message;

    return 0;
}

int 
PnpMessage_SetMessage(
    _In_ PNPMESSAGE Message,
    _In_ const char* payload
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg;
    
    assert(NULL != Message);
    
    msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);

    // Allocate memory for the payload
    int length = (int) strlen(payload);
    int size = (length + 1) * sizeof(char);

    msg->MessageLength = length + 1;
    msg->Message = (char*)malloc(size);

    // Copy the payload
    strcpy_s((char*)msg->Message, msg->MessageLength, payload);

    return 0;
}

const char* 
PnpMessage_GetMessage(
    _In_ PNPMESSAGE Message
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg;

    assert(NULL != Message);
    
    msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);

    return (char*) msg->Message;
}

int 
PnpMessage_SetInterfaceId(
    PNPMESSAGE Message,
    const char* InterfaceId
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg;
    
    assert(NULL != Message);

    msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);

    // Allocate memory for the payload
    int length = (int) strlen(InterfaceId);
    int size = (length + 1) * sizeof(char);

    msg->InterfaceId = (char*)malloc(size);

    // Copy the payload
    strcpy_s(msg->InterfaceId, size, InterfaceId);

    return 0;
}

const char*
PnpMessage_GetInterfaceId(
    _In_ PNPMESSAGE Message
    ) 
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;
    
    assert(NULL != Message);

    msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);
    return msg->InterfaceId;
}

PNPMESSAGE_PROPERTIES* 
PnpMessage_AccessProperties(
    _In_ PNPMESSAGE Message
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;
    
    assert(NULL != Message);

    msg = (PPNPBRIDGE_CHANGE_PAYLOAD)PnpMemory_GetBuffer(Message, NULL);
    return &msg->Properties;
}

void
PnpMessage_AddReference(
    _In_ PNPMESSAGE Message
    )
{
    PnpMemory_AddReference(Message);
}

void
PnpMessage_ReleaseReference(
    _In_ PNPMESSAGE Message
    )
{
    PnpMemory_ReleaseReference(Message);
}

int 
PnpMessageQueue_Worker(
    _In_ void* threadArgument
    )
{
    PMESSAGE_QUEUE queue = NULL;

    queue = (PMESSAGE_QUEUE) threadArgument;

    while (true)
    {
        PNPMESSAGE message = NULL;

        // TODO: check status
        Lock(queue->WaitConditionLock);

        if (queue->TearDown) {
            Unlock(queue->WaitConditionLock);
            ThreadAPI_Exit(0);
        }

        if (0 == queue->InCount) {
            Condition_Wait(queue->WaitCondition, queue->WaitConditionLock, 0);
        }

        if (queue->TearDown) {
            Unlock(queue->WaitConditionLock);
            ThreadAPI_Exit(0);
        }

        Unlock(queue->WaitConditionLock);

        // Due to SDK limitation of not being able to incrementally publish
        // interface unload all Pnp adapters and re-init the interface
        // Steps for this:
        // 1. Unload all the pnp adapters
        // 2. Destroy the Digital twin client
        // 3. Recreate Digital twin client
        // 4. Replay all messages from devices reported by discovery adapters
        // 5. Publish the interface

        PnpBridge_ReinitPnpAdapters();

        // Move messages from ingress queue to publish queue
        while (true) {
            PnpMesssageQueue_Remove(queue, &message);
            if (NULL == message) {
                break;
            }

            PnpMesssageQueue_AddToPublishQ(queue, message);
        }

        PDLIST_ENTRY head = &queue->PublishQueue;
        PDLIST_ENTRY current = head->Flink;
        while (current->Blink != head->Blink) {
            PDLIST_ENTRY next = current->Flink;
            PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;

            msg = containingRecord(current, PNPBRIDGE_CHANGE_PAYLOAD, Entry);
            message = msg->Self;
            if (message) {
                // Process the message
                if (PNPBRIDGE_OK != PnpBridge_ProcessPnpMessage(message)) {
                    DList_RemoveEntryList(current);
                    PnpMessage_ReleaseReference(message);
                }
            }
            current = next;
        }

        // Query all the pnp interface clients and publish them
        DiscoveryAdapter_PublishInterfaces();
    }

    return 0;
}

PNPBRIDGE_RESULT
PnpMessageQueue_Create(
    _In_ PMESSAGE_QUEUE* PnpMessageQueue
    )
{
    PMESSAGE_QUEUE queue = NULL;
    PNPBRIDGE_RESULT result = PNPBRIDGE_OK;

    TRY 
    {
        queue = calloc(1, sizeof(MESSAGE_QUEUE));
        if (NULL == queue) {
            LogError("Failed to allocate memory for the pnpmessage queue");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        queue->TearDown = false;

        queue->QueueLock = Lock_Init();
        if (NULL == queue->QueueLock) {
            LogError("Failed to queue lock");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        queue->WaitConditionLock = Lock_Init();
        if (NULL == queue->WaitConditionLock) {
            LogError("Failed to queue WaitConditionLock");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        queue->WaitCondition = Condition_Init();
        if (NULL == queue->WaitCondition) {
            LogError("Failed to queue WaitCondition");
            result = PNPBRIDGE_INSUFFICIENT_MEMORY;
            LEAVE;
        }

        DList_InitializeListHead(&queue->IngressQueue);

        DList_InitializeListHead(&queue->PublishQueue);

        if (THREADAPI_OK != ThreadAPI_Create(&queue->Worker, PnpMessageQueue_Worker, queue)) {
            LogError("Failed to create PnpMessageQueue_Worker thread");
            queue->Worker = NULL;
            LEAVE;
        }

        *PnpMessageQueue = queue;
    }
    FINALLY
    {
        if (PNPBRIDGE_OK != result) {
            if (queue) {
                PnpMessageQueue_Release(queue);
            }
        }
    }

    return result;
}

PNPBRIDGE_RESULT 
PnpMesssageQueue_Add(
    PMESSAGE_QUEUE Queue,
    PNPMESSAGE PnpMessage
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;
    
    msg = (PPNPBRIDGE_CHANGE_PAYLOAD) PnpMemory_GetBuffer(PnpMessage, NULL);

    // Take reference on PNPMESSAGE
    PnpMemory_AddReference(PnpMessage);

    // Acquire queue lock
    Lock(Queue->QueueLock);

    DList_AppendTailList(&Queue->IngressQueue, &msg->Entry);

    Queue->InCount++;

    // Release the queue lock
    Unlock(Queue->QueueLock);

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT
PnpMesssageQueue_AddToPublishQ(
    _In_ PMESSAGE_QUEUE Queue,
    _In_ PNPMESSAGE PnpMessage
    )
{
    PPNPBRIDGE_CHANGE_PAYLOAD msg;
    
    msg = (PPNPBRIDGE_CHANGE_PAYLOAD) PnpMemory_GetBuffer(PnpMessage, NULL);

    DList_InitializeListHead(&msg->Entry);

    DList_AppendTailList(&Queue->PublishQueue, &msg->Entry);

    Queue->PublishCount++;

    return PNPBRIDGE_OK;
}

PNPBRIDGE_RESULT 
PnpMesssageQueue_Remove(
    _In_ PMESSAGE_QUEUE Queue,
    _Out_ PNPMESSAGE* PnpMessage
    )
{
    *PnpMessage = NULL;

    Lock(Queue->QueueLock);

    if (!DList_IsListEmpty(&Queue->IngressQueue)) {
        // Dequeue a PnpMessage
        PDLIST_ENTRY currentEntry = DList_RemoveHeadList(&Queue->IngressQueue);

        PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;
        if (NULL != currentEntry) {
            msg = containingRecord(currentEntry, PNPBRIDGE_CHANGE_PAYLOAD, Entry);
            *PnpMessage = msg->Self;
            Queue->InCount--;
        }
    }

    Unlock(Queue->QueueLock);

    return PNPBRIDGE_OK;
}

void
PnpMessageQueue_Release(
    _In_ PMESSAGE_QUEUE Queue
    )
{
    assert(NULL != Queue);

    Queue->TearDown = true;

    Lock(Queue->WaitConditionLock);
    Condition_Post(Queue->WaitCondition);
    Unlock(Queue->WaitConditionLock);

    // Wait for message queue worker to complete
    if (NULL != Queue->Worker) {
        ThreadAPI_Join(Queue->Worker, NULL);
    }

    // Release all the PNPMESSAGE's
    if (!DList_IsListEmpty(&Queue->PublishQueue)) {
        PDLIST_ENTRY currentEntry = DList_RemoveHeadList(&Queue->PublishQueue);

        PPNPBRIDGE_CHANGE_PAYLOAD msg = NULL;
        if (NULL != currentEntry) {
            msg = containingRecord(currentEntry, PNPBRIDGE_CHANGE_PAYLOAD, Entry);
            PnpMemory_ReleaseReference(msg->Self);
        }
    }

    if (NULL != Queue->QueueLock) {
        Lock_Deinit(Queue->QueueLock);
    }

    if (NULL != Queue->WaitCondition) {
        Condition_Deinit(Queue->WaitCondition);
    }

    if (NULL != Queue->WaitConditionLock) {
        Lock_Deinit(Queue->WaitConditionLock);
    }
}