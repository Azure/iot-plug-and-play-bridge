// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This application uses the Azure IoT Hub device SDK for .NET
// For samples see: https://github.com/Azure/azure-iot-sdk-csharp/tree/master/iothub/device/samples

using System;
using Microsoft.Azure.Devices.Client;
using Newtonsoft.Json;
using System.Text;
using System.Threading.Tasks;


namespace PnpGateway
{
    class Gateway
    {
        // The device connection string to authenticate the device with your IoT hub.
        // Using the Azure CLI:
        // az iot hub device-identity show-connection-string --hub-name {YourIoTHubName} --device-id MyDotnetDevice --output table
        private readonly static string s_connectionString = "HostName=npn-hub.azure-devices.net;DeviceId=stm32;SharedAccessKey=WxtA1QkSSUVioe98+imbllmcBC0nPUnynUhrQ3/Q6Zs=";

        private static void Main(string[] args)
        {

            Console.WriteLine("Conneting to hub and updating pnp device interfaces.\n");
            //new MyPnpInterface().DoWork(s_connectionString);

            

            var deviceClient = DeviceClient.CreateFromConnectionString(s_connectionString, TransportType.Mqtt);

            var pnpDeviceClient = new PnpDeviceClient(deviceClient);
            pnpDeviceClient.Initialize().Wait();


            var dev = new Device("COM4", pnpDeviceClient);
            Console.ReadLine();

            return;
        }
    }
}
