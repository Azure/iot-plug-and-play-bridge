// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pnpbridge.h"
#include "azure_c_shared_utility/xlogging.h"

int main()
{
    LogInfo("\n --Running Azure IoT PnP Bridge as an IoT Edge Runtime Module\n");
    PnpBridge_Main(NULL);
    return 0;
}
