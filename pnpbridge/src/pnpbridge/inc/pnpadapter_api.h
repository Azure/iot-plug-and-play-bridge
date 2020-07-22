// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef PNPBRIDGE_PNP_ADAPTER_INTERFACE_H
#define PNPBRIDGE_PNP_ADAPTER_INTERFACE_H

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"
#include "parson.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Pnp interface handle
    typedef void* PNPBRIDGE_INTERFACE_HANDLE;
    typedef PNPBRIDGE_INTERFACE_HANDLE* PPNPBRIDGE_INTERFACE_HANDLE;

    // Pnp adapter handle
    typedef void* PNPBRIDGE_ADAPTER_HANDLE;
    typedef PNPBRIDGE_ADAPTER_HANDLE* PPNPBRIDGE_ADAPTER_HANDLE;

    /*
    * @brief    Create is the adapter callback which allocates and initializes the adapter 
    *           context on the adapter handle. The adapter may call PnpAdapterHandleSetContext 
    *           after successfully creating and setting up the adapter context. The adapter must
    *           free up context if a failure occurs. Destroy will not be called if Create fails.
    *           Adapters are not statically created and initialized, instead this API will only
    *           be invoked for adapters with device in the config which requires the corresponding 
    *           adapter to function correctly. Therefore, a failed return value from this call
    *           will be fatal and prevent further execution of the pnp bridge.
    *
    * @param    AdapterGlobalConfig         Global parameters for a pnp adapter specified in 
                                            config (pnp_bridge_adapter_global_configs). This is a JSON
    *                                       object structure that can be parsed using json_object_ and 
                                            json_value APIs. This can be empty if the adapter does not
                                            require additional config parameters
    *
    * @param    AdapterHandle               Handle to the pnp adapter
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_ADAPTER_CREATE) (const JSON_Object* AdapterGlobalConfig, 
        PNPBRIDGE_ADAPTER_HANDLE AdapterHandle);


    /*
    * @brief    PNPBRIDGE_ADAPTER_DESTOY is the callback to deallocate and deinitialize the pnp adapter context 
                that it set up during adapter Create (PNPBRIDGE_ADAPTER_CREATE). PNPBRIDGE_ADAPTER_DESTOY
                will be called on every adapter that was successfully created by a call to PNPBRIDGE_ADAPTER_CREATE.
                It is not called when an adapter fails PNPBRIDGE_ADAPTER_CREATE.
    *
    *
    * @param    AdapterHandle              Handle to the pnp adapter
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_ADAPTER_DESTOY)(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle);


    /*
    * @brief    PNPBRIDGE_INTERFACE_CREATE callback is called to create a new azure pnp interface client
    *           and bind all the interface callback functions with the digital twin client. The pnp adapter
    *           should call DigitalTwin_InterfaceClient_Create to create the interface client and bind the
    *           DigitalTwin_InterfaceClient_SetPropertiesUpdatedCallback and
    *           DigitalTwin_InterfaceClient_SetCommandsCallback. The adapter must be ready to receive 
    *           commands or property updates once it returns from this callback. The adapter should
    *           wait until PNPBRIDGE_INTERFACE_START callback to start reporting telemetry. This call is
    *           made once for each component in the configuration file.
    *
    *
    * @param    ComponentName             Component name of the interface from the config file.
    *
    * @param    AdapterHandle             Pnp adapter handle that is associated with the interface.
    *                                     Adapter can call PnpAdapterHandleGetContext to get the adapter
                                          context which it may have set previously
    *
    *
    * @param    AdapterInterfaceConfig    Adapter config that is interface specific (pnp_bridge_adapter_config)
                                          from the config file
    *
    * @param    PnpInterfaceClient        DIGITALTWIN_INTERFACE_CLIENT_HANDLE populated from Pnp SDK's 
                                          DigitalTwin_InterfaceClient_Create
    *
    * @param    PnpInterfaceHandle        Handle to Pnp interface
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure. If this call fails the bridge
    *           will consider it fatal and fail to start. If this call succeeds it is expected that 
    *           PnpInterfaceClient is set to a valid DIGITALTWIN_INTERFACE_CLIENT_HANDLE.
    */

    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_INTERFACE_CREATE)(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle, 
        const char* ComponentName, const JSON_Object* AdapterInterfaceConfig,
        PNPBRIDGE_INTERFACE_HANDLE BridgeInterfaceHandle);

    /*
    * @brief    PNPBRIDGE_INTERFACE_START starts the pnp interface after it has been registered with Azure Pnp.
    *           The pnp adapter should ensure that the interface starts reporting telmetry after this call is 
    *           made and not before it. This call is made once for each component in the configuration file.
    *
    * @param    PnpInterfaceHandle    Handle to Pnp interface
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */

    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_INTERFACE_START)(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle, 
        PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle);

    /*
    * @brief    PNPBRIDGE_INTERFACE_STOP cleans up all telemetry resouces
    *           that is associated with this interface. The interface should stop reporting telemetry but
    *           the pnp adpater should be able to handle property update and command callbacks after this
    *           call returns. This call is made once for each component in the configuration file.
    *
    * @param    PnpInterfaceHandle    Handle to Pnp interface
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_INTERFACE_STOP)(PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle);

    /*
    * @brief    PNPBRIDGE_INTERFACE_DESTROY cleans up the pnp interface context and calls 
    *           DigitalTwin_InterfaceClient_Destroy on the interface. This call is made once for each 
    *           component in the configuration file.
    *
    *
    * @param    PnpInterfaceHandle    Handle to Pnp interface
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_INTERFACE_DESTROY)(PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle);


    /*
    * @brief    PNPBRIDGE_ADAPTER_PROPERTY_UPDATE calls the property update callback for
    *           a certain adapter type based on the interface handle
    *
    *
    * @param    PnpInterfaceHandle      Handle to Pnp interface
    *
    * @param    PropertyName            Name of property for which property updated callback needs to be called
    *
    * @param    PropertyValue           JSON_Value Property update
    *
    * @param    version                 Version
    *
    * @param    userContextCallback     User context callback
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef void (*PNPBRIDGE_ADAPTER_PROPERTY_UPDATE)(PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle,
        const char* PropertyName, JSON_Value* PropertyValue, int version, void* userContextCallback);
    
    /*
    * @brief    PNPBRIDGE_ADAPTER_PROCESS_COMMAND calls the process command callback for
    *           a certain adapter type based on the interface handle
    *
    *
    * @param    PnpInterfaceHandle     Handle to Pnp interface
    *
    * @param    CommandName            Name of command which needs to be processed
    *
    * @param    CommandValue           JSON_Value Command root value
    *
    * @param    CommandResponse        The resonse to the command which the adapter allocates and populates
    *
    * @param    CommandResponseSize    Size of the command response buffer
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef int (*PNPBRIDGE_ADAPTER_PROCESS_COMMAND)(PNPBRIDGE_INTERFACE_HANDLE PnpInterfaceHandle,
        const char* CommandName, JSON_Value* CommandValue, unsigned char** CommandResponse, size_t* CommandResponseSize);

    /*
        PnpAdapter and Interface Methods
    */

    /**
    * @brief    PnpAdapterHandleSetContext sets adapter context on an adapter handle. If a context was already set 
    *           from a previous call to PnpAdapterHandleSetContext, the context set from the latest call will be retained

    * @param    AdapterHandle          Handle to pnp adapter
    *
    * @param    AdapterContext         Context to pnp adapter
    *
    * @returns  void  PnpAdapterHandleSetContext will always overwrite the adapter context successfully
    */
    MOCKABLE_FUNCTION(,
        void,
        PnpAdapterHandleSetContext,
        PNPBRIDGE_ADAPTER_HANDLE, AdapterHandle,
        void*, AdapterContext
    );

    /**
    * @brief    PnpAdapterHandleGetContext gets adapter context from an adapter handle

    * @param    AdapterHandle          Handle to pnp adapter
    *
    * @returns  void *                 Adapter context
    */
    MOCKABLE_FUNCTION(,
        void*,
        PnpAdapterHandleGetContext,
        PNPBRIDGE_ADAPTER_HANDLE, AdapterHandle
    );

    /**
    * @brief    PnpInterfaceHandleSetContext sets context on an interface handle

    * @param    InterfaceHandle          Handle to pnp interface
    *
    * @param    InterfaceContext         Interface context to set
    *
    * @returns  void  PnpInterfaceHandleSetContext will always overwrite the interface context
    */
    MOCKABLE_FUNCTION(,
        void,
        PnpInterfaceHandleSetContext,
        PNPBRIDGE_INTERFACE_HANDLE, InterfaceHandle,
        void*, InterfaceContext
    );

    /**
    * @brief    PnpInterfaceHandleGetContext gets interface context from an interface handle

    * @param    InterfaceHandle        Handle to pnp interface
    *
    * @returns  void *                 Interface context
    */
    MOCKABLE_FUNCTION(,
        void*,
        PnpInterfaceHandleGetContext,
        PNPBRIDGE_INTERFACE_HANDLE, InterfaceHandle
    );

    
    /**
    * @brief    PnpInterfaceHandleGetContext gets adapter context from an adapter handle

    * @param    InterfaceHandle               Handle to pnp interface
    *
    * @returns  IOTHUB_DEVICE_CLIENT_HANDLE   IoTHub Device Client handle
    */
    MOCKABLE_FUNCTION(,
        IOTHUB_DEVICE_CLIENT_HANDLE,
        PnpInterfaceHandleGetIotHubDeviceClient,
        PNPBRIDGE_INTERFACE_HANDLE, InterfaceHandle
    );


    /*
        PnpAdapter Binding info
    */
    typedef struct _PNP_ADAPTER {
        // Identity of the pnp adapter that is retrieved from the config
        const char* identity;

        PNPBRIDGE_ADAPTER_CREATE createAdapter;
        PNPBRIDGE_INTERFACE_CREATE createPnpInterface;
        PNPBRIDGE_INTERFACE_START startPnpInterface;
        PNPBRIDGE_INTERFACE_STOP stopPnpInterface;
        PNPBRIDGE_INTERFACE_DESTROY destroyPnpInterface;
        PNPBRIDGE_ADAPTER_DESTOY destroyAdapter;
        PNPBRIDGE_ADAPTER_PROPERTY_UPDATE processPropertyUpdate;
        PNPBRIDGE_ADAPTER_PROCESS_COMMAND processCommand;
    } PNP_ADAPTER, * PPNP_ADAPTER;

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_PNP_ADAPTER_INTERFACE_H */