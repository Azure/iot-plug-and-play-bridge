// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#else
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#endif

static void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

static void* my_gballoc_calloc(size_t num, size_t size)
{
    return calloc(num, size);
}

static void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#include "testrunnerswitcher.h"

#include "azure_c_shared_utility/macro_utils.h"
#include "umock_c.h"
#include "umock_c_negative_tests.h"
#include "../../../serializer/tests/serializer_dt_ut/real_crt_abstractions.h"

#define ENABLE_MOCKS
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/strings.h"
#include "iothub_module_client.h"
#include "internal/pnp_client_core.h"
#include "internal/lock_thread_binding_impl.h"


//#include "pnp_module_client.h"

#include "pnpbridge.h"
#include "pnpbridge_common.h"
#include "discovery_manager.h"
#undef ENABLE_MOCKS

DISCOVERY_ADAPTER EnvironmentSensorDiscovery = {
    .Identity = "environment-sensor-sample-discovery-adapter",
    .StartDiscovery = NULL,
    .StopDiscovery = NULL
};

PDISCOVERY_ADAPTER DISCOVERY_ADAPTER_MANIFEST[] = {
    &EnvironmentSensorDiscovery
};

const int DiscoveryAdapterCount = sizeof(DISCOVERY_ADAPTER_MANIFEST) / sizeof(PDISCOVERY_ADAPTER);

TEST_DEFINE_ENUM_TYPE(PNP_CLIENT_RESULT, PNP_CLIENT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(PNP_CLIENT_RESULT, PNP_CLIENT_RESULT_VALUES);

#define PNP_TEST_INTERFACE_HANDLE1 ((PNP_INTERFACE_CLIENT_HANDLE)0x1001)
#define PNP_TEST_INTERFACE_HANDLE2 ((PNP_INTERFACE_CLIENT_HANDLE)0x1002)
#define PNP_TEST_INTERFACE_HANDLE3 ((PNP_INTERFACE_CLIENT_HANDLE)0x1003)

static PNP_INTERFACE_CLIENT_HANDLE testPnPInterfacesToRegister[] = {PNP_TEST_INTERFACE_HANDLE1, PNP_TEST_INTERFACE_HANDLE2, PNP_TEST_INTERFACE_HANDLE3 };
static const int testPnPInterfacesToRegisterLen = sizeof(testPnPInterfacesToRegister) / sizeof(testPnPInterfacesToRegister[0]);
static void* pnpTestModuleRegisterInterfaceCallbackContext = (void*)0x1236;

static const IOTHUB_MODULE_CLIENT_HANDLE testIotHubModuleHandle = (IOTHUB_MODULE_CLIENT_HANDLE)0x1236;

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

// Create a custom mock function to allocate a single byte (with a corresponding free mock function).
// This will make sure when Valgrind runs, it would catch handle leaks (which it wouldn't if this was the default mock)
static PNP_CLIENT_CORE_HANDLE test_PnP_ClientCore_Create(PNP_IOTHUB_BINDING* iotHubBinding)
{
    (void)iotHubBinding;
    return (PNP_CLIENT_CORE_HANDLE)my_gballoc_malloc(1);
}

static void test_PnP_ClientCore_Destroy(PNP_CLIENT_CORE_HANDLE pnpClientCoreHandle)
{
    (void)pnpClientCoreHandle;
    my_gballoc_free(pnpClientCoreHandle);
}

BEGIN_TEST_SUITE(pnpbridge_discovery_manager_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result);

    REGISTER_UMOCK_ALIAS_TYPE(PNP_CLIENT_CORE_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(PNP_INTERFACE_REGISTERED_CALLBACK, void*);


    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, my_gballoc_malloc);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(gballoc_malloc, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_calloc, my_gballoc_calloc);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(gballoc_calloc, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, my_gballoc_free);

    REGISTER_GLOBAL_MOCK_HOOK(PnP_ClientCore_Create, test_PnP_ClientCore_Create);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(PnP_ClientCore_Create, NULL);
    REGISTER_GLOBAL_MOCK_HOOK(PnP_ClientCore_Destroy, test_PnP_ClientCore_Destroy);
    REGISTER_GLOBAL_MOCK_RETURN(PnP_ClientCore_RegisterInterfacesAsync, PNP_CLIENT_OK);
    REGISTER_GLOBAL_MOCK_FAIL_RETURN(PnP_ClientCore_RegisterInterfacesAsync, PNP_CLIENT_ERROR);
}

TEST_FUNCTION_INITIALIZE(TestMethodInit)
{
    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(TestMethodCleanup)
{
    //;
}

static void set_expected_calls_for_PnpAdapterManager_InitializeAdapter()
{
    //STRICT_EXPECTED_CALL(PnP_ClientCore_Create(IGNORED_PTR_ARG));
}

///////////////////////////////////////////////////////////////////////////////
// PnpAdapterManager_InitializeAdapter
///////////////////////////////////////////////////////////////////////////////
TEST_FUNCTION(PnpAdapterManager_InitializeAdapter_ok)
{
    // arrange
    //PPNP_ADAPTER_TAG h;
    //PPNP_ADAPTER pnpAdapter;
    
    PDISCOVERY_MANAGER discoveryMgr;
    PNPBRIDGE_RESULT result;
    //set_expected_calls_for_PnpAdapterManager_InitializeAdapter();

    //act
    result = DiscoveryAdapterManager_Create(&discoveryMgr);

    //assert    
    ASSERT_IS_NOT_NULL(discoveryMgr);
    //ASSERT_ARE_EQUAL(PNPBRIDGE_RESULT, result, PNPBRIDGE_OK);
        
    //cleanup
    DiscoveryAdapterManager_Release(discoveryMgr);
}

// TEST_FUNCTION(PnP_ModuleClient_CreateFromModuleHandle_NULL_handle_fails)
// {
//     // arrange
//     PNP_MODULE_CLIENT_HANDLE h;

//     //act
//     h = PnP_ModuleClient_CreateFromModuleHandle(NULL);

//     //assert    
//     ASSERT_IS_NULL(h);
// }

// TEST_FUNCTION(PnP_ModuleClient_CreateFromModuleHandle_fail)
// {
//     // arrange
//     int negativeTestsInitResult = umock_c_negative_tests_init();
//     ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

//     PNP_MODULE_CLIENT_HANDLE h;

//     set_expected_calls_for_PnP_ModuleClient_CreateFromModuleHandle();
//     umock_c_negative_tests_snapshot();

//     size_t count = umock_c_negative_tests_call_count();
//     for (size_t i = 0; i < count; i++)
//     {
//         char message[256];
//         sprintf(message, "PnP_ModuleClient_CreateFromModuleHandle failure in test %lu", (unsigned long)i);

//         umock_c_negative_tests_reset();
//         umock_c_negative_tests_fail_call(i);

//         h = PnP_ModuleClient_CreateFromModuleHandle(testIotHubModuleHandle);
        
//         //assert
//         ASSERT_IS_NULL(h);
//     }

//     //cleanup
//     umock_c_negative_tests_deinit();
// }


// static PNP_MODULE_CLIENT_HANDLE allocate_PNP_MODULE_CLIENT_HANDLE_for_test()
// {
//     PNP_MODULE_CLIENT_HANDLE h = PnP_ModuleClient_CreateFromModuleHandle(testIotHubModuleHandle);
//     ASSERT_IS_NOT_NULL(h);
//     umock_c_reset_all_calls();
//     return h;
// }

// ///////////////////////////////////////////////////////////////////////////////
// // PnP_ModuleClient_Destroy
// ///////////////////////////////////////////////////////////////////////////////
// static void set_expected_calls_for_PnP_ModuleClient_Destroy()
// {
//     STRICT_EXPECTED_CALL(PnP_ClientCore_Destroy(IGNORED_PTR_ARG));
// }

// TEST_FUNCTION(PnP_ModuleClient_Destroy_ok)
// {
//     //arrange
//     PNP_MODULE_CLIENT_HANDLE h = allocate_PNP_MODULE_CLIENT_HANDLE_for_test();
//     set_expected_calls_for_PnP_ModuleClient_Destroy();

//     //act
//     PnP_ModuleClient_Destroy(h);

//     //assert
//     ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
// }

// ///////////////////////////////////////////////////////////////////////////////
// // PnP_ModuleClient_RegisterInterfacesAsync
// ///////////////////////////////////////////////////////////////////////////////
// static void pnpTestModuleRegisterInterfaceCallback(PNP_REPORTED_INTERFACES_STATUS pnpInterfaceStatus, void* userContextCallback)
// {
//     (void)pnpInterfaceStatus;
//     (void)userContextCallback;
// }

// static void set_expected_calls_for_PnP_ModuleClient_RegisterInterfacesAsync()
// {
//     STRICT_EXPECTED_CALL(PnP_ClientCore_RegisterInterfacesAsync(IGNORED_PTR_ARG, testPnPInterfacesToRegister, testPnPInterfacesToRegisterLen, pnpTestModuleRegisterInterfaceCallback, pnpTestModuleRegisterInterfaceCallbackContext));
// }

// TEST_FUNCTION(PnP_ModuleClient_RegisterInterfacesAsync_ok)
// {
//     // arrange
//     PNP_MODULE_CLIENT_HANDLE h = allocate_PNP_MODULE_CLIENT_HANDLE_for_test();
//     PNP_CLIENT_RESULT result;

//     //act
//     set_expected_calls_for_PnP_ModuleClient_RegisterInterfacesAsync();
//     result = PnP_ModuleClient_RegisterInterfacesAsync(h, testPnPInterfacesToRegister, testPnPInterfacesToRegisterLen, pnpTestModuleRegisterInterfaceCallback, pnpTestModuleRegisterInterfaceCallbackContext);

//     //assert
//     ASSERT_ARE_EQUAL(PNP_CLIENT_RESULT, result, PNP_CLIENT_OK);
//     ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
        
//     //cleanup
//     PnP_ModuleClient_Destroy(h);
// }

// TEST_FUNCTION(PnP_ModuleClient_RegisterInterfacesAsync_fail)
// {
//     // arrange
//     int negativeTestsInitResult = umock_c_negative_tests_init();
//     ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

//     PNP_MODULE_CLIENT_HANDLE h = allocate_PNP_MODULE_CLIENT_HANDLE_for_test();
//     PNP_CLIENT_RESULT result;

//     set_expected_calls_for_PnP_ModuleClient_RegisterInterfacesAsync();
//     umock_c_negative_tests_snapshot();

//     size_t count = umock_c_negative_tests_call_count();
//     for (size_t i = 0; i < count; i++)
//     {
//         umock_c_negative_tests_reset();
//         umock_c_negative_tests_fail_call(i);

//         //act
//         result = PnP_ModuleClient_RegisterInterfacesAsync(h, testPnPInterfacesToRegister, testPnPInterfacesToRegisterLen, pnpTestModuleRegisterInterfaceCallback, pnpTestModuleRegisterInterfaceCallbackContext);
        
//         //assert
//         ASSERT_ARE_NOT_EQUAL(PNP_CLIENT_RESULT, result, PNP_CLIENT_OK, "PnP_ModuleClient_RegisterInterfacesAsync failure in test %lu", (unsigned long)i);
//     }

//     //cleanup
//     umock_c_negative_tests_deinit();
//     PnP_ModuleClient_Destroy(h);
// }

END_TEST_SUITE(pnpbridge_discovery_manager_ut)

