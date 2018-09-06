// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// This application uses the Azure IoT Hub service SDK for .NET
// For samples see: https://github.com/Azure/azure-iot-sdk-csharp/tree/master/iothub/service
using System;
using Microsoft.Azure.Devices;
using System.Threading;

namespace iotpnp_service
{
    class IotPnpService
    {
        private static ServiceClient s_serviceClient;
        
        // Connection string for your IoT Hub
        // az iot hub show-connection-string --hub-name {your iot hub name}Endpoint=sb://ihsuprodbyres062dednamespace.servicebus.windows.net/;SharedAccessKeyName=iothubowner;SharedAccessKey=ONNNpoZkJ/Z7Qb6z/z9K9kAzqREgoUnNvPOGIa6jKD0=
        private readonly static string s_connectionString = "HostName=npn-hub.azure-devices.net;Endpoint=sb://ihsuprodbyres062dednamespace.servicebus.windows.net/;SharedAccessKeyName=iothubowner;SharedAccessKey=ONNNpoZkJ/Z7Qb6z/z9K9kAzqREgoUnNvPOGIa6jKD0=";
      
        private static void PrintUsage()
        {
            Console.WriteLine("");
            Console.WriteLine("Parameters:");
            Console.WriteLine("l: list all interfaces published by the device");
            Console.WriteLine("Perform an operation. Eg:<interfaceid> <operationtype> <commandname> <commandinput>");
            Console.WriteLine("    <interfaceid>: Id for the interface obtained by listing all interfaces");
            Console.WriteLine("    <OperationType>:");
            Console.WriteLine("        pr - property read");
            Console.WriteLine("        pw - property write");
            Console.WriteLine("        c  - send command");
            Console.WriteLine("        e  - listen to events");
            Console.WriteLine("");
            Console.WriteLine("    <commandname>: Name of the property/command");
            Console.WriteLine();
            Console.WriteLine("Press q to exit anytime.");
        }

        private static void Main(string[] args)
        {
            Console.WriteLine("Azure IOT PNP - COSINE CLI.\n");

            var pnpMgr = new PnpInterfaceManger(s_connectionString);

            PrintUsage();

            while (true)
            {
                string input = Console.ReadLine();
                Console.WriteLine();
                if (input == "q")
                {
                    break;
                }

                if (input == "" || input == null)
                {
                    continue;
                }

                if (input == "l")
                {
                    pnpMgr.DumpInterfaces().Wait();
                    continue;
                }

                try
                {
                    var cmd_split = input.TrimEnd().Split(" ");
                    if (cmd_split.Length < 3 || cmd_split.Length > 4)
                    {
                        Console.WriteLine("Invalid command format");
                        continue;
                    }

                    string interfaceId = cmd_split[0];
                    string cmdType = cmd_split[1];
                    string cmdName = cmd_split[2];
                    string cmdInput = "";

                    if (cmd_split.Length == 4) { 
                        cmdInput = cmd_split[3];
                    }

                    var t = pnpMgr.GetPnpInterface(interfaceId);
                    t.Wait();
                    var pnpInt = t.Result;
                    if (pnpInt != null)
                    {
                        if (cmdType == "pr")
                        {
                            var t1 = pnpInt.ReadProperty(cmdName);
                            t1.Wait();
                            Console.WriteLine("Property value: " + t1.Result);
                        }
                        else if (cmdType == "pw")
                        {
                            var t1 = pnpInt.WriteProperty(cmdName, input);
                            t1.Wait();
                            Console.WriteLine("Property updated");
                        }
                        else if (cmdType == "e")
                        {
                            CancellationTokenSource cts = new CancellationTokenSource();
                            Console.WriteLine("Press CTRL+C to stop listening");

                            Console.CancelKeyPress += (s, e) =>
                            {
                                e.Cancel = true;
                                cts.Cancel();
                                Console.WriteLine("Stopped Listening...");
                            };
                            pnpInt.ListenToEvent(cmdName, cts).Wait();
                        }
                        else if (cmdType == "c")
                        {
                            var t1 = pnpInt.SendCommand(cmdName, cmdInput);
                            t1.Wait();
                            Console.WriteLine("command output: " + t1.Result);
                        }
                    }
                    else
                    {
                        Console.WriteLine("Interface not found: " + interfaceId);
                    }
                    continue;
                }
                catch (Exception ex)
                {
                    Console.WriteLine("Encountered error: " + ex.Message);
                    Console.WriteLine(ex.StackTrace);
                    PrintUsage();
                }
            }
        
        }
    }
}
