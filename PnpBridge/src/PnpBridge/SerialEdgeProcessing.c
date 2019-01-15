#include "common.h"

#include "azure_c_shared_utility/base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/lock.h"

typedef enum SERIAL_EDGE_PROCESSING_ACTION {
	MOTOR_ON = 0,
	MOTOR_OFF
} SERIAL_EDGE_PROCESSING_ACTION;

typedef struct _SERIAL_EDGE_PROCESSING_CONTEXT {
	PNP_INTERFACE_CLIENT_HANDLE pnpInterface;
	char* eventName;
	char* data;
	SERIAL_EDGE_PROCESSING_ACTION Action;
} SERIAL_EDGE_PROCESSING_CONTEXT, *PSERIAL_EDGE_PROCESSING_CONTEXT;

int DeviceAggregator_Serial_EdgeProcessing(PNP_INTERFACE_CLIENT_HANDLE pnpInterface, char* eventName, char* data) {
	// Check against the rules

	// copy eventName, data and keep reference of pnpInterface
	PSERIAL_EDGE_PROCESSING_CONTEXT edgeProcessContext = calloc(sizeof(SERIAL_EDGE_PROCESSING_CONTEXT), 1);
	edgeProcessContext->eventName = NULL;
	edgeProcessContext->data = NULL;
	edgeProcessContext->pnpInterface = pnpInterface;

	// Start a thread with above context and run a a command on pnpInterface


	return 0;
}