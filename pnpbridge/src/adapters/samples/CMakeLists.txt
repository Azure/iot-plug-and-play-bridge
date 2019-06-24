#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

#this is CMakeLists.txt for PnP samples.

#usePermissiveRulesForSdkSamplesAndTests()

# function(add_sample_directory whatIsBuilding)
    # add_subdirectory(${whatIsBuilding})

    # set_target_properties(${whatIsBuilding}
               # PROPERTIES
               # FOLDER "PnPBridge_Adapters_Samples")
# endfunction()

# add_sample_directory(environment_sensor)

add_subdirectory(environmental_sensor)