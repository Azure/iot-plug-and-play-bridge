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


//#include "pnp_module_client.h"

#include "pnpbridge.h"
#include "pnpbridge_common.h"
#include "discoveryadapter_manager.h"
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

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%d", error_code);
    ASSERT_FAIL(temp_str);
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

BEGIN_TEST_SUITE(pnpbridge_discovery_manager_ut)

TEST_SUITE_INITIALIZE(suite_init)
{
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

END_TEST_SUITE(pnpbridge_discovery_manager_ut)

