#pragma once
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // PnpAdapterManger's representation of a PNPADPTER and its configuration
    typedef struct _PNP_ADAPTER_TAG {
        PPNP_ADAPTER adapter;

        // List of pnp interfaces created under this adapter
        SINGLYLINKEDLIST_HANDLE pnpInterfaceList;

        // Lock to protect pnpInterfaceList modification
        LOCK_HANDLE InterfaceListLock;
    } PNP_ADAPTER_TAG, * PPNP_ADAPTER_TAG;

    // Structure uses to share context between adapter manager and adapter interface
    typedef struct _PNP_ADAPTER_CONTEXT_TAG {
        PPNPBRIDGE_ADAPTER_HANDLE context;
        PPNP_ADAPTER_TAG adapter;
        JSON_Object* adapterGlobalConfig;
    } PNP_ADAPTER_CONTEXT_TAG, * PPNP_ADAPTER_CONTEXT_TAG;

    // Structure used for an instance of Pnp Adapter Manager
    typedef struct _PNP_ADAPTER_MANAGER {
        unsigned int NumInterfaces;
        SINGLYLINKEDLIST_HANDLE PnpAdapterHandleList;
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE* PnpInterfacesRegistrationList;
    } PNP_ADAPTER_MANAGER, * PPNP_ADAPTER_MANAGER;


    // Pnp interface structure
    typedef struct _PNPADAPTER_INTERFACE_TAG {
        void* context;
        const char* interfaceName;
        const char* adapterIdentity;
        DIGITALTWIN_INTERFACE_CLIENT_HANDLE pnpInterfaceClient;
    } PNPADAPTER_INTERFACE_TAG, * PPNPADAPTER_INTERFACE_TAG;


    /**
    * @brief    PnpAdapterManager_CreateManager creates the Azure Pnp Interface adapter manager
    *
    * @remarks  PnpAdapterManager_Create initializes all the available Pnp adapters by calling their
                initialize method, if implemented

    * @param    adapter           Pointer to get back an initialized PPNP_ADAPTER_MANAGER
    *
    * @returns  DIGITALTWIN_CLIENT_OK on success and other DIGITALTWIN_CLIENT_RESULT values on failure
    */
    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateManager(
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
    * @brief    PnpAdapterManager_ReleaseAdapterInterfaces calls each adapter's clean up methods to 
                release pnp interfaces that were set up by them
    *
    * @remarks  PnpAdapterManager_ReleaseAdapterInterfaces calls stopPnpInterface and
                destroyPnpInterface on each created interface

    * @param    adapter           Pointer to an initialized PPNP_ADAPTER_TAG
    *
    * @returns  VOID
    */
    void PnpAdapterManager_ReleaseAdapterInterfaces(
        PPNP_ADAPTER_TAG adapterTag);

    // PnpAdapterManager utility functions

    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_GetAdapterFromManifest(
        const char* adapterId,
        PPNP_ADAPTER* adapter);
    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateAdapter(
        const char* adapterId,
        PPNP_ADAPTER_CONTEXT_TAG* adapterContext,
        JSON_Value* config);
    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_CreateInterfaces(
        PPNP_ADAPTER_MANAGER adapterMgr,
        JSON_Value* config);
    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_GetAdapterHandle(
        PPNP_ADAPTER_MANAGER adapterMgr,
        const char* adapterIdentity,
        PPNP_ADAPTER_CONTEXT_TAG* adapterContext);
    DIGITALTWIN_CLIENT_RESULT PnPAdapterManager_RegisterInterfaces(
        PPNP_ADAPTER_MANAGER adapterMgr);
    DIGITALTWIN_CLIENT_RESULT PnpAdapterManager_StartInterfaces(
        PPNP_ADAPTER_MANAGER adapterMgr);

#ifdef __cplusplus
}
#endif