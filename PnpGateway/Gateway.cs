// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This application uses the Azure IoT Hub device SDK for .NET
// For samples see: https://github.com/Azure/azure-iot-sdk-csharp/tree/master/iothub/device/samples

using System;
using Microsoft.Azure.Devices.Client;

namespace PnpGateway
{
    class Gateway
    {
        // The device connection string to authenticate the device with your IoT hub.
        // Using the Azure CLI:
        // az iot hub device-identity show-connection-string --hub-name {YourIoTHubName} --device-id MyDotnetDevice --output table
        private readonly static string s_connectionString = "HostName=iot-pnp-hub1.azure-devices.net;DeviceId=win-gateway;SharedAccessKey=GfbYy7e2PikTf2qHyabvEDBaJB5S4T+H+b9TbLsXfns=";

        private static void Main(string[] args)
        {

            Console.WriteLine("Conneting to hub and updating pnp device interfaces.\n");

            var deviceClient = DeviceClient.CreateFromConnectionString(s_connectionString, TransportType.Mqtt);

            var pnpDeviceClient = new PnpDeviceClient(deviceClient);
            pnpDeviceClient.Initialize().Wait();

            // Discover USB devices
            var usbdevicemgmt = new UsbDeviceManagement(pnpDeviceClient);
            usbdevicemgmt.StartMonitoring();

            // Discover serial interfaces
            var dev = new Device("COM4", pnpDeviceClient);
            Console.ReadLine();

            return;
        }
    }
}
