// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef PNPBRIDGE_PNP_ADAPTER_INTERFACE_H
#define PNPBRIDGE_PNP_ADAPTER_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

// Pnp adapter interface handle
typedef void* PNPADAPTER_INTERFACE_HANDLE;
typedef PNPADAPTER_INTERFACE_HANDLE* PPNPADAPTER_INTERFACE_HANDLE;
typedef void* PNPADAPTER_CONTEXT;

/**
* @brief    PNPADAPTER_PNP_INTERFACE_INITIALIZE callback uses to initialize a pnp adapter.
*

* @param    adapterArgs           Json object containing pnp adapter parameters specified in config.
*
* @returns  integer greater than zero on success and other values on failure.
*/
typedef int(*PNPADAPTER_PNP_INTERFACE_INITIALIZE) (const char* adapterArgs);


/**
* @brief    PNPADAPTER_PNP_INTERFACE_SHUTDOWN callback to uninitialize the pnp adapter.
*

*
* @returns  integer greater than zero on success and other values on failure.
*/
typedef int(*PNPADAPTER_PNP_INTERFACE_SHUTDOWN)();

/**
* @brief    PNPADAPTER_BIND_PNP_INTERFACE callback is called to create a new azure pnp interface client.
*

* @param    pnpInterface               Handle to pnp adapter interface
*
* @param    pnpDeviceClientHandle      Handle to pnp device client
*
* @param    payload                    Payload containing device info that was discovered
*
* @returns  integer greater than zero on success and other values on failure.
*/
//typedef int(*PNPADAPTER_BIND_PNP_INTERFACE)(PNPADAPTER_INTERFACE_HANDLE pnpInterface, PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle, PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload);

typedef int(*PNPADAPTER_BIND_PNP_INTERFACE)(PNPADAPTER_CONTEXT adapterHandle, 
                                            PNP_DEVICE_CLIENT_HANDLE pnpDeviceClientHandle,
                                            PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload);

// NOTES TO SELF:

//
// Config update requires bridge tear down and restart
//

// Discovered Device -> Bind -> CreatePnpInterfaces -> Create Pnp Interface client ->
// Create Pnp Adapter Interface -> Associate it with the adapter and store the device config pointer -> Get All interface for all adapters -> publish
//

// ***************

/**
* @brief    PNPADAPTER_RELEASE_PNP_INTERFACE uninitializes the pnp interface.
*
* @remarks  For private preview, azure pnp adapter should call PnP_InterfaceClient_Destroy

* @param    pnpInterface    Handle to pnp adapter interface
*
* @returns  integer greater than zero on success and other values on failure.
*/
typedef int(*PNPADAPTER_INTERFACE_RELEASE)(PNPADAPTER_INTERFACE_HANDLE pnpInterface);


typedef int(*PNPADAPTER_DEVICE_ARRIVED)(PPNPBRIDGE_DEVICE_CHANGE_PAYLOAD payload);

/**
   PnpAdapterInterface Methods
**/

// Pnp adapter interface creation parameters
typedef struct _PNPADPATER_INTERFACE_INIT_PARAMS {
    PNPADAPTER_DEVICE_ARRIVED deviceArrived;
	PNPADAPTER_INTERFACE_RELEASE releaseInterface;
} PNPADPATER_INTERFACE_INIT_PARAMS, *PPNPADPATER_INTERFACE_INIT_PARAMS;

/**
* @brief    PnpAdapter_CreatePnpInterface creates a PnP Bridge adapter interface

* @param    pnpAdapterInterface          Handle to pnp adapter interface
*
* @returns  Result indicating status of pnp adapter interface creation
*/
int PnpAdapterInterface_Create(PNPADAPTER_CONTEXT adapterHandle, const char* interfaceName,
                                  PNP_INTERFACE_CLIENT_HANDLE Interface,
                                  PPNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface,
								  PPNPADPATER_INTERFACE_INIT_PARAMS params);

/**
* @brief    PnpAdapter_DestroyPnpInterface destroys a PnP Bridge adapter interface

* @param    pnpAdapterInterface          Handle to pnp adapter interface
*
*/
void PnpAdapterInterface_Destroy(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface);

/**
* @brief    PnpAdapter_GetPnpInterfaceClient gets the Azure iot pnp interface client handle

* @param    pnpInterface          Handle to pnp adapter interface
*
* @returns  Handle to Azure Pnp Interface client
*/
PNP_INTERFACE_CLIENT_HANDLE PnpAdapterInterface_GetPnpInterfaceClient(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface);

/**
* @brief    PnpAdapter_SetContext sets a context for pnp adapter interface handle

* @param    pnpInterface          Handle to pnp adapter interface
*
* @param    Context               Pointer to a context
*
* @returns  integer greater than zero on success and other values on failure.
*/
int PnpAdapterInterface_SetContext(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface, void* context);

/**
* @brief    PnpAdapter_GetContext gets context set by pnp adapter.
*
* @remarks  PnpAdapter_GetContext is used to get the context set using PnpAdapter_SetContext

* @param    pnpInterface         Handle to pnp adapter interface
*
* @returns  void* context
*/
void* PnpAdapterInterface_GetContext(PNPADAPTER_INTERFACE_HANDLE pnpAdapterInterface);

/*
	PnpAdapter Binding info
*/
typedef struct _PNP_ADAPTER {
    // Identity of the Pnp Adapter that will be used in the config 
    // of a device under PnpParameters
    const char* identity;

    PNPADAPTER_PNP_INTERFACE_INITIALIZE initialize;

	PNPADAPTER_BIND_PNP_INTERFACE createPnpInterface;

	//PNPADAPTER_RELEASE_PNP_INTERFACE releaseInterface;

	PNPADAPTER_PNP_INTERFACE_SHUTDOWN shutdown;
} PNP_ADAPTER, *PPNP_ADAPTER;

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_PNP_ADAPTER_INTERFACE_H */