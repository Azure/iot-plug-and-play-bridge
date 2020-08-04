// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifndef PNPBRIDGE_PNP_ADAPTER_INTERFACE_H
#define PNPBRIDGE_PNP_ADAPTER_INTERFACE_H

#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"
#include "parson.h"
#include <iothub_device_client.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Pnp component handle
    typedef void* PNPBRIDGE_COMPONENT_HANDLE;
    typedef PNPBRIDGE_COMPONENT_HANDLE* PPNPBRIDGE_COMPONENT_HANDLE;

    // Pnp adapter handle
    typedef void* PNPBRIDGE_ADAPTER_HANDLE;
    typedef PNPBRIDGE_ADAPTER_HANDLE* PPNPBRIDGE_ADAPTER_HANDLE;

    // Process Command Callback
    typedef int(*PNPBRIDGE_COMPONENT_METHOD_CALLBACK)(
        PNPBRIDGE_COMPONENT_HANDLE componentHandle,
        const char* CommandName,
        JSON_Value* CommandValue,
        unsigned char** CommandResponse,
        size_t* CommandResponseSize);

    // Process Property Update Callback
    typedef void(*PNPBRIDGE_COMPONENT_PROPERTY_CALLBACK)(
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
        const char* PropertyName,
        JSON_Value* PropertyValue,
        int version,
        void* userContextCallback);

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
    * @brief    PNPBRIDGE_COMPONENT_CREATE callback is invoked by the pnp bridge to create and allocate 
    *           device context, set up component resources to handle property update and command callbacks
    *           from the cloud and bind the callback functions to process property updates and commands
    *           for the associated component. After the internal creation of component resources in the
    *           PNPBRIDGE_COMPONENT_CREATE callback, the pnp adapter should set device context on the component
    *           handle by calling PnpComponentHandleSetContext, bind a callback to handle property updates on
    *           the component by calling PnpComponentHandleSetPropertyUpdateCallback and bind a callback to
    *           The adapter must be ready to receive commands or property updates once it returns 
    *           from this callback. The adapter should wait until PNPBRIDGE_COMPONENT_START callback to
    *           start reporting telemetry. PNPBRIDGE_COMPONENT_CREATE is invoked once for each component
    *           in the configuration file.
    *
    *
    * @param    ComponentName             Unique name of the component from the configuration file
    *
    * @param    AdapterHandle             Pnp adapter handle that is associated with the component.
    *                                     Adapter can call PnpAdapterHandleGetContext to get the adapter
                                          context which it may have set previously
    *
    *
    * @param    AdapterComponentConfig    Adapter config that is component specific (pnp_bridge_adapter_config)
                                          from the config file
    *
    * @param    PnpComponentHandle        Handle to Pnp bridge component
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure. If this call fails the bridge
    *           will consider it fatal and fail to start.
    */

    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_COMPONENT_CREATE)(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle, 
        const char* ComponentName, const JSON_Object* AdapterComponentConfig,
        PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle);

    /*
    * @brief    PNPBRIDGE_COMPONENT_START callback is invoked for each component in the configuration
    *           file after the IoT hub device client handle associated with the root interface of the
    *           pnp bridge has been registered with IoT hub. The pnp adapter should ensure that the component
    *           starts reporting telenetry after this call is made and not before it.
    *
    * @param    PnpComponentHandle    Handle to Pnp component
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */

    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_COMPONENT_START)(PNPBRIDGE_ADAPTER_HANDLE AdapterHandle, 
        PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

    /*
    * @brief    PNPBRIDGE_COMPONENT_STOP cleans up all telemetry resouces
    *           that is associated with this component. The component should stop reporting telemetry but
    *           the pnp adpater should be able to handle property update and command callbacks after this
    *           call returns. This call is made once for each component in the configuration file.
    *
    * @param    PnpComponentHandle    Handle to Pnp component
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_COMPONENT_STOP)(PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

    /*
    * @brief    PNPBRIDGE_COMPONENT_DESTROY cleans up the pnp component's device context and other
    *           resources managed by the adapter that was created during PNPBRIDGE_COMPONENT_CREATE
    *           This call is made once for each component in the configuration file.
    *
    *
    * @param    PnpComponentHandle    Handle to Pnp component
    *
    * @returns  IOTHUB_CLIENT_OK on success and other values on failure
    */
    typedef IOTHUB_CLIENT_RESULT(*PNPBRIDGE_COMPONENT_DESTROY)(PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle);

    /*
        PnpAdapter and Interface Methods
    */

    /**
    * @brief    PnpAdapterHandleSetContext sets adapter context on an adapter handle. If a context was already set 
    *           from a previous call to PnpAdapterHandleSetContext, the context set from the latest call is retained

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
    * @brief    PnpComponentHandleSetContext sets context on a component handle

    * @param    ComponentHandle          Handle to pnp component
    *
    * @param    ComponentDeviceContext   Component's device context to set
    *
    * @returns  void  PnpComponentHandleSetContext will always overwrite the device context
    */
    MOCKABLE_FUNCTION(,
        void,
        PnpComponentHandleSetContext,
        PNPBRIDGE_COMPONENT_HANDLE, ComponentHandle,
        void*, ComponentDeviceContext
    );

    /**
    * @brief    PnpComponentHandleGetContext gets context from a component handle

    * @param    ComponentHandle        Handle to pnp component
    *
    * @returns  void *                 Component's device context
    */
    MOCKABLE_FUNCTION(,
        void*,
        PnpComponentHandleGetContext,
        PNPBRIDGE_COMPONENT_HANDLE, ComponentHandle
    );

    /**
    * @brief    PnpComponentHandleSetPropertyUpdateCallback sets property update callback on the
    *           component handle to manage cloud to device property updates per component.
    *           PnpComponentHandleSetPropertyUpdateCallback should be called from a 
    *           PNPBRIDGE_COMPONENT_CREATE callback

    * @param    ComponentHandle        Handle to pnp component
    *
    * @param    PropertyUpdateCallback Property update Callback
    * 
    * @returns  void   PnpComponentHandleSetPropertyUpdateCallback will always overwrite successfully
    */
    MOCKABLE_FUNCTION(,
        void,
        PnpComponentHandleSetPropertyUpdateCallback,
        PNPBRIDGE_COMPONENT_HANDLE, ComponentHandle,
        PNPBRIDGE_COMPONENT_PROPERTY_CALLBACK, PropertyUpdateCallback
    );

    /**
    * @brief    PnpComponentHandleSetCommandCallback sets process command callback on the
    *           component handle to manage cloud to device command updates per component.
    *           PnpComponentHandleSetCommandCallback should be called from a 
    *           PNPBRIDGE_COMPONENT_CREATE callback

    * @param    ComponentHandle        Handle to pnp component
    *
    * @param    CommandCallback        Process command callback
    * 
    * @returns  void   PnpComponentHandleSetCommandCallback will always overwrite successfully
    */
    MOCKABLE_FUNCTION(,
        void,
        PnpComponentHandleSetCommandCallback,
        PNPBRIDGE_COMPONENT_HANDLE, ComponentHandle,
        PNPBRIDGE_COMPONENT_METHOD_CALLBACK, CommandCallback
    );

    /**
    * @brief    PnpComponentHandleGetIotHubDeviceClient gets device client handle from the component handle

    * @param    ComponentHandle               Handle to pnp component
    *
    * @returns  IOTHUB_DEVICE_CLIENT_HANDLE   IoTHub Device Client handle
    */
    MOCKABLE_FUNCTION(,
        IOTHUB_DEVICE_CLIENT_HANDLE,
        PnpComponentHandleGetIotHubDeviceClient,
        PNPBRIDGE_COMPONENT_HANDLE, ComponentHandle
    );


    /*
        PnpAdapter Binding info
    */
    typedef struct _PNP_ADAPTER {
        // Identity of the pnp adapter that is retrieved from the config
        const char* identity;

        PNPBRIDGE_ADAPTER_CREATE createAdapter;
        PNPBRIDGE_COMPONENT_CREATE createPnpComponent;
        PNPBRIDGE_COMPONENT_START startPnpComponent;
        PNPBRIDGE_COMPONENT_STOP stopPnpComponent;
        PNPBRIDGE_COMPONENT_DESTROY destroyPnpComponent;
        PNPBRIDGE_ADAPTER_DESTOY destroyAdapter;
    } PNP_ADAPTER, * PPNP_ADAPTER;

#ifdef __cplusplus
}
#endif

#endif /* PNPBRIDGE_PNP_ADAPTER_INTERFACE_H */