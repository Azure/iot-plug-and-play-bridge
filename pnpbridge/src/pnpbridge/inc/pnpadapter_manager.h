// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once
#include "pnpadapter_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // PnpAdapterManger's representation of a PNPADPTER and its configuration
    typedef struct _PNP_ADAPTER_TAG
    {
        // Pnp Adapter with bound interfaces
        PPNP_ADAPTER adapter;

        // List of pnp components created under this adapter
        SINGLYLINKEDLIST_HANDLE PnpComponentList;

        // Lock to protect PnpComponentList modification
        LOCK_HANDLE ComponentListLock;

    } PNP_ADAPTER_TAG, * PPNP_ADAPTER_TAG;

    // Structure used to share context between adapter manager and adapter interface
    typedef struct _PNP_ADAPTER_CONTEXT_TAG {
        PPNPBRIDGE_ADAPTER_HANDLE context;
        PPNP_ADAPTER_TAG adapter;
        JSON_Object* adapterGlobalConfig;
    } PNP_ADAPTER_CONTEXT_TAG, * PPNP_ADAPTER_CONTEXT_TAG;

    // Structure used for an instance of Pnp Adapter Manager
    typedef struct _PNP_ADAPTER_MANAGER {
        unsigned int NumComponents;
        SINGLYLINKEDLIST_HANDLE PnpAdapterHandleList;
        char ** ComponentsInModel;
    } PNP_ADAPTER_MANAGER, * PPNP_ADAPTER_MANAGER;


    // Pnp interface structure
    typedef struct _ {
        void* context;
        char* componentName;
        char* adapterIdentity;
        PNPBRIDGE_COMPONENT_PROPERTY_PATCH_CALLBACK processPropertyPatch;
        PNPBRIDGE_COMPONENT_PROPERTY_COMPLETE_CALLBACK processPropertyComplete;
        PNPBRIDGE_COMPONENT_METHOD_CALLBACK processCommand;
        PNP_BRIDGE_CLIENT_HANDLE clientHandle;
        PNP_BRIDGE_IOT_TYPE clientType;
    } PNPADAPTER_COMPONENT_TAG, * PPNPADAPTER_COMPONENT_TAG;


    /**
    * @brief    PnpAdapterManager_CreateManager creates the Azure Pnp Interface adapter manager
    *
    * @remarks  PnpAdapterManager_Create initializes all the available Pnp adapters by calling their
                initialize method, if implemented

    * @param    adapter           Pointer to get back an initialized PPNP_ADAPTER_MANAGER
    *
    * @returns  IOTHUB_CLIENT_OK on success and other IOTHUB_CLIENT_RESULT values on failure
    */
    IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateManager(
        PPNP_ADAPTER_MANAGER* adapter,
        JSON_Value* config);

    /**
    * @brief    PnpAdapterManager_Release uninitializes the PnpAdapterMgr resources
    *
    * @remarks  PnpAdapterManager_Release calls shutdown on all the Pnp adapters, its interfaces 
                and cleans up the PnpAdapterManager

    * @param    adapter           Pointer to an initialized PPNP_ADAPTER_MANAGER
    *
    * @returns  VOID
    */
    void PnpAdapterManager_ReleaseManager(
        PPNP_ADAPTER_MANAGER adapter);


    /**
    * @brief    PnpAdapterManager_ReleaseAdapterComponents calls each adapter's clean up methods to 
                release pnp interfaces that were set up by them
    *
    * @remarks  PnpAdapterManager_ReleaseAdapterComponents calls stopPnpComponent and
                destroyPnpComponent on each created interface

    * @param    adapter           Pointer to an initialized PPNP_ADAPTER_TAG
    *
    * @returns  VOID
    */
    void PnpAdapterManager_ReleaseAdapterComponents(
        PPNP_ADAPTER_TAG adapterTag);

    // PnpAdapterManager utility functions

    IOTHUB_CLIENT_RESULT PnpAdapterManager_GetAdapterFromManifest(
        const char* adapterId,
        PPNP_ADAPTER* adapter);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateAdapter(
        const char* adapterId,
        PPNP_ADAPTER_CONTEXT_TAG* adapterContext,
        JSON_Value* config);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_CreateComponents(
        PPNP_ADAPTER_MANAGER adapterMgr,
        JSON_Value* config,
        PNP_BRIDGE_IOT_TYPE clientType);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_GetAdapterHandle(
        PPNP_ADAPTER_MANAGER adapterMgr,
        const char* adapterIdentity,
        PPNP_ADAPTER_CONTEXT_TAG* adapterContext);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_InitializeClientHandle(
        PPNPADAPTER_COMPONENT_TAG componentHandle);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_StartComponents(
        PPNP_ADAPTER_MANAGER adapterMgr);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_BuildComponentsInModel(
        PPNP_ADAPTER_MANAGER adapterMgr);
    
    IOTHUB_CLIENT_RESULT PnpAdapterManager_StopComponents(
        PPNP_ADAPTER_MANAGER adapterMgr);
    
    IOTHUB_CLIENT_RESULT PnpAdapterManager_DestroyComponents(
        PPNP_ADAPTER_MANAGER adapterMgr);
    
    void PnpAdapterManager_ReleaseComponentsInModel(
        PPNP_ADAPTER_MANAGER adapterMgr);

    PPNPADAPTER_COMPONENT_TAG PnpAdapterManager_GetComponentHandleFromComponentName(
        const char * ComponentName,
        size_t ComponentNameSize);

    IOTHUB_CLIENT_RESULT PnpAdapterManager_BuildAdaptersAndComponents(
        PPNP_ADAPTER_MANAGER * adapterMgr,
        JSON_Value* config,
        PNP_BRIDGE_IOT_TYPE clientType);

    // Device Twin callback is invoked by IoT SDK when a twin - either full twin or a PATCH update - arrives.
    void PnpAdapterManager_DeviceTwinCallback(
        DEVICE_TWIN_UPDATE_STATE updateState,
        const unsigned char* payload,
        size_t size,
        void* userContextCallback);

    // Device Method callback is invoked by IoT SDK when a device method arrives.
    int PnpAdapterManager_DeviceMethodCallback(
        const char* methodName,
        const unsigned char* payload,
        size_t size,
        unsigned char** response,
        size_t* responseSize,
        void* userContextCallback);

    // PnpAdapterManager_RoutePropertyPatchCallback is the callback function that the PnP helper layer routes per property update for DEVICE_TWIN_UPDATE_PARTIAL.
    static void PnpAdapterManager_RoutePropertyPatchCallback(
        const char* componentName,
        const char* propertyName,
        JSON_Value* propertyValue,
        int version,
        void* userContextCallback);

    // PnpAdapterManager_RoutePropertyCompleteCallback is the callback function that the PnP helper layer routes per property update for DEVICE_TWIN_UPDATE_COMPLETE.
    static bool PnpAdapterManager_RoutePropertyCompleteCallback(
        const unsigned char *payload,
        size_t size,
        void *userContextCallback);

    static void PnpAdapterManager_ResumePnpBridgeAdapterAndComponentCreation(
        JSON_Value* pnpBridgeConfig);
    
    void PnpAdapterManager_SendPnpBridgeStateTelemetry(
        const char * BridgeState);

    static const char* PnpBridge_State = "ModuleState";
    static const char* PnpBridge_WaitingForConfig = "Waiting for PnpBridge configuration property update";
    static const char* PnpBridge_ConfigurationComplete = "PnpBridge configuration complete";

#ifdef __cplusplus
}
#endif