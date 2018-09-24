using Microsoft.Azure.Devices;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;

namespace iotpnp_service
{
    public class UsbPnpInterface
    {
        public string FriendlyName { get; set; }

        public string DeviceInterface { get; set; }

        public async Task InvokeMethod(ServiceClient serviceClient, string method, int deviceIndex)
        {
            var methodInvocation = new CloudToDeviceMethod(method) { ResponseTimeout = TimeSpan.FromSeconds(30) };
            methodInvocation.SetPayloadJson(deviceIndex.ToString());

            // Invoke the direct method asynchronously and get the response from the simulated device.
            var response = await serviceClient.InvokeDeviceMethodAsync("win-gateway", methodInvocation);

            Console.WriteLine("Response status: {0}, payload:", response.Status);
            Console.WriteLine(response.GetPayloadAsJson());
        }
    }

    public class UsbDevices
    {
        private readonly static string s_connectionString = "HostName=npn-hub.azure-devices.net;Endpoint=sb://ihsuprodbyres062dednamespace.servicebus.windows.net/;SharedAccessKeyName=iothubowner;SharedAccessKey=ONNNpoZkJ/Z7Qb6z/z9K9kAzqREgoUnNvPOGIa6jKD0=";

        public async void UsbDevicesInit()
        {
            Loaded = false;
            RegistryManager registryManager = RegistryManager.CreateFromConnectionString(s_connectionString);
            var query = registryManager.CreateQuery("SELECT * FROM devices", 100);
            while (query.HasMoreResults)
            {
                var page = await query.GetNextAsTwinAsync();
                foreach (var twin in page)
                {
                    string usbprop = twin.Properties.Reported["IoT-PnP-UsbDeviceManagement"];
                    // do work on twin object
                    Devices = JsonConvert.DeserializeObject<List<UsbPnpInterface>>(usbprop);
                    break;
                }

                Loaded = true;
                Console.WriteLine("Device twin loaded. Found " + Devices.Count + " USB devices");

                Console.WriteLine("");
                Console.WriteLine("Parameters:");
                Console.WriteLine("l: List usb device");
                Console.WriteLine("r: update usb device list from device twin");
                Console.WriteLine("c: run a command. Supported commands are enable/disable and select device using s param. Eg: s 1 c enable");
            }
        }

        public bool Loaded = false;

        public List<UsbPnpInterface> Devices;

        internal void PrintDevices()
        {
            for (int i = 0; i < Devices.Count; i++)
            {
                Console.WriteLine(i + ". " + Devices[i].FriendlyName);
            }
        }
    }
}
