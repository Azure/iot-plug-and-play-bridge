using Microsoft.Azure.Devices.Client;
using Microsoft.Azure.Devices.Shared;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PnpGateway
{
    public class PnpDeviceClient
    {
        public PnpDeviceClient(DeviceClient deviceClient)
        {
            DeviceClient = deviceClient;
            PnpInterfaceList = new Dictionary<string, PnPInterface>();
        }

        public async Task Initialize()
        {
            // Clear the interface list
            Twin dt = await DeviceClient.GetTwinAsync();
            TwinCollection t = dt.Properties.Reported;
            if (t.Contains("PnpInterfaces"))
            {
                TwinCollection tc = new TwinCollection();
                tc["PnpInterfaces"] = JsonConvert.SerializeObject(new List<string>());
                await DeviceClient.UpdateReportedPropertiesAsync(tc);
            }
        }

        private Dictionary<string, PnPInterface> PnpInterfaceList { get; set; }

        public async Task PublishInterface(PnPInterface pnpInterface)
        {
            if (PnpInterfaceList.Keys.Contains(pnpInterface.Id))
            {
                throw new InvalidOperationException("Interface is already published");
            }

            TwinCollection settings = new TwinCollection();

            var json = pnpInterface.GetJson();
            settings[pnpInterface.Id] = json;

            PnpInterfaceList.Add(pnpInterface.Id, pnpInterface);
            settings["PnpInterfaces"] = JsonConvert.SerializeObject(PnpInterfaceList.Keys.ToList());
            await DeviceClient.UpdateReportedPropertiesAsync(settings);

            await DeviceClient.SetMethodHandlerAsync("PnpCommandHandler"+ pnpInterface.Id, pnpInterface.PnpInterfaceCommandHandler, null);
        }

        public DeviceClient DeviceClient { get; private set; }
    }
}
