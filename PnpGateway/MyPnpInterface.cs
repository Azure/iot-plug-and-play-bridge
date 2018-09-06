using Microsoft.Azure.Devices.Client;
using System;

namespace PnpGateway
{

    class MyPnpInterface
    {
        public string CmdHandler(string cmd, string Input)
        {
            Console.WriteLine(cmd + " " + Input);
            return "Done";
        }

        public string PropChange(string propName, string Input)
        {
            Console.WriteLine(propName + " " + Input);
            return "Done";
        }

        public async void DoWork(string ConnectionString)
        {
            // Connect to the IoT hub using the MQTT protocol
            var deviceClient = DeviceClient.CreateFromConnectionString(ConnectionString, TransportType.Mqtt);

            var pnpDeviceClient = new PnpDeviceClient(deviceClient);
            await pnpDeviceClient.Initialize();

            // Add a new interface
            var pnpInterface = new PnPInterface("peninterface", pnpDeviceClient, this.PropChange, this.CmdHandler);
            pnpInterface.BindProperty("prop1");

            pnpInterface.BindCommand("cmd1");

            pnpInterface.BindProperty("prop2");
            pnpInterface.BindEvent("event1");

            await pnpDeviceClient.PublishInterface(pnpInterface);

            await pnpInterface.SendEvent("event1", "10");

            pnpInterface = new PnPInterface("rubberinterface", pnpDeviceClient, this.PropChange, this.CmdHandler);
            pnpInterface.BindProperty("prop1");

            pnpInterface.BindCommand("cmd1");
           // await pnpDeviceClient.PublishInterface(pnpInterface);
        }
    }
}
