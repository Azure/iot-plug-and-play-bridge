// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This application uses the Azure IoT Hub device SDK for .NET
// For samples see: https://github.com/Azure/azure-iot-sdk-csharp/tree/master/iothub/device/samples

using System;
using Microsoft.Azure.Devices.Client;
using PnpGateway.Serial;

namespace PnpGateway
{
    class Gateway
    {
        // The device connection string to authenticate the device with your IoT hub.
        // Using the Azure CLI:
        // az iot hub device-identity show-connection-string --hub-name {YourIoTHubName} --device-id MyDotnetDevice --output table
        //"HostName=iot-pnp-hub1.azure-devices.net;DeviceId=win-gateway;SharedAccessKey=GfbYy7e2PikTf2qHyabvEDBaJB5S4T+H+b9TbLsXfns=";
        //"HostName=iot-pnp-hub1.azure-devices.net;DeviceId=win-gateway;SharedAccessKey=GfbYy7e2PikTf2qHyabvEDBaJB5S4T+H+b9TbLsXfns=";
        private readonly static string s_connectionString = "HostName=saas-iothub-1529564b-8f58-4871-b721-fe9459308cb1.azure-devices.net;DeviceId=956da476-8b3c-41ce-b405-d2d32bcf5e79;SharedAccessKey=sQcfPeDCZGEJWPI3M3SyB8pD60TNdOw10oFKuv5FBio=";

        private static void Main(string[] args)
        {
            Console.WriteLine("Azure IoT Device Aggregator");

            Console.WriteLine("Conneting to Azure IoT hub ..\n");

            var deviceClient = DeviceClient.CreateFromConnectionString(s_connectionString, TransportType.Mqtt);

            var pnpDeviceClient = new PnpDeviceClient(deviceClient);
            pnpDeviceClient.Initialize().Wait();

            Console.WriteLine("Connected to Azure IoT Device\n");

            // Discover USB devices
            var usbdevicemgmt = new UsbDeviceManagement(pnpDeviceClient);
            usbdevicemgmt.StartMonitoring();

            // Discover serial interfaces
            var dev = new SerialPnPDevice("COM5", pnpDeviceClient);
            dev.Start();
            Console.ReadLine();

            return;
        }
    }
}
