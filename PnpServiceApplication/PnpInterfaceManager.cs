using Microsoft.Azure.Devices;
using Microsoft.Azure.Devices.Shared;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace iotpnp_service
{
    public class PnpInterfaceManger
    {
        public PnpInterfaceManger(string connectionString)
        {
            ConnectionString = connectionString;
        }

        private readonly string ConnectionString;

        public async Task<PnpInterface> GetPnpInterface(string id)
        {
            Twin twin = await GetDeviceTwin();
            if (twin.Properties.Reported.Contains("PnpInterfaces"))
            {
                List<string> interfaces = JsonConvert.DeserializeObject<List<string>>(twin.Properties.Reported["PnpInterfaces"].ToString());
                if (interfaces.Contains(id))
                {
                    Newtonsoft.Json.Linq.JObject json = JObject.Parse(twin.Properties.Reported[id].ToString());

                    var pnpInt = new PnpInterface(json, ConnectionString);
                    return pnpInt;
                }
            }

            return null;
        }

        private async Task<Twin> GetDeviceTwin()
        {
            RegistryManager registryManager = RegistryManager.CreateFromConnectionString(ConnectionString);
            Twin t = await registryManager.GetTwinAsync("stm32");
            return t;
        }

        public async Task DumpInterfaces()
        {
            Twin twin = await GetDeviceTwin();
            if (twin.Properties.Reported.Contains("PnpInterfaces"))
            {
                // Get all interfaces under device twin and iterate through them
                List<string> interfaces = JsonConvert.DeserializeObject<List<string>>(twin.Properties.Reported["PnpInterfaces"].ToString());
                foreach (var intId in interfaces)
                {

                    JObject pnpInt = JObject.Parse(twin.Properties.Reported[intId].ToString());

                    Console.WriteLine("InterfaceId: " + pnpInt["id"]);

                    // Print properties
                    JObject props = (JObject)pnpInt["Properties"];
                    if (props.Count > 0)
                    {
                        Console.WriteLine("properties: ");
                        foreach (var prop in props)
                        {
                            Console.WriteLine("    " + prop.Key);
                        }
                    }

                    // Print events
                    JArray events = (JArray)pnpInt["Events"];
                    if (events.Count > 0)
                    {
                        Console.WriteLine("events: ");
                        foreach (var ev in events)
                        {
                            Console.WriteLine("    " + ev.ToString());
                        }
                    }

                    // Print commands
                    JObject commands = (JObject)pnpInt["Commands"];
                    if (commands.Count > 0)
                    {
                        Console.WriteLine("commands: ");
                        foreach (var cm in commands)
                        {
                            Console.WriteLine("    " + cm.Key);
                        }
                    }
                    Console.WriteLine();
                }
            }
        }
    }
}


