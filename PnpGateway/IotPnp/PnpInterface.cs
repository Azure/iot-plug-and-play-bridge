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
    public delegate string PropertyChangeHandler(string propName, string input);

    public class PnPInterface
    {
        public string Id { get; private set; }

        private DeviceClient DeviceClient;

        Dictionary<string, string> Properties;
        private List<CommandDef> Commands;
        private List<string> Events;
        public PropertyChangeHandler PropChangeHandler { get; private set; }
        public CommandHandler CmdHandler { get; private set; }

        public PnPInterface(string id, PnpDeviceClient pnpDeviceClient, PropertyChangeHandler propHandler, CommandHandler commandHandler)
        {
            Id = id;
            DeviceClient = pnpDeviceClient.DeviceClient;
            Properties = new Dictionary<string, string>();
            Commands = new List<CommandDef>();
            Events = new List<string>();
            PropChangeHandler = propHandler;
            CmdHandler = commandHandler;
        }

        public string GetJson()
        {
            JObject pnpInterface =
                   new JObject(
                   new JProperty("id", Id),
                   new JProperty("Events",
                       new JArray(
                           from p in Events
                           select new JValue(p))),
                   new JProperty("Properties",
                       new JObject(
                           from p in Properties.Keys
                           select new JProperty(p, Properties[p]))),
                   new JProperty("Commands",
                       new JObject(
                           from p in Commands.Select(x => x.Command)
                           select new JProperty(p, ""))));

            return JsonConvert.SerializeObject(pnpInterface);
        }

        public void BindProperty(string property)
        {
            Properties[property] = "";
        }

        public void BindEvent(string property)
        {
            Events.Add(property);
        }

        public Task<MethodResponse> PnpInterfaceCommandHandler(MethodRequest methodRequest, object userContext)
        {
            string result;
            var data = Encoding.UTF8.GetString(methodRequest.Data);
            JObject json = JObject.Parse(data);

            var cmdDescriptor = json["command"];
            var cmdType = cmdDescriptor["type"].ToString();
            var cmdName = cmdDescriptor["name"].ToString();

            var interfaceId = json["interfaceid"].ToString();

            var input = json["input"]?.ToString();

            if (interfaceId != Id)
            {
                result = "{\"result\":\"Invalid interface Id\"}";
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 400));
            }

            if (cmdType == "propread")
            {
                if (Properties.ContainsKey(cmdName))
                {
                    result = "{\"result\":\""+ Properties[cmdName] + "\"}";
                    return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 200));
                }
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes("Property not found"), 400));
            }
            else if (cmdType == "propwrite")
            {
                WriteProperty(cmdName, input);
                result = "{\"result\":\"Success\"}";
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 200));
            }
            else if (cmdType == "cmd")
            {
                var ret = CmdHandler(cmdName, input);
                result = "{\"result\":\"" + ret + "\"}";
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 200));
            }

            result = "{\"result\":\"Invalid parameter\"}";
            return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 400));
        }

        public void BindCommand(string Command)
        {
            if (Commands.Select(x => x.Command).Contains(Command))
            {
                throw new InvalidOperationException("command already exists");
            }
            Commands.Add(new CommandDef(Command, CmdHandler));
        }

        public void WriteProperty(string property, object val)
        {
            if (!Properties.ContainsKey(property))
            {
                throw new InvalidOperationException("Property doesn't exist");
            }

            PropChangeHandler(property, val.ToString());
        }

        private async Task<JObject> GetInterfaceFromTwin()
        {
            Twin dt = await DeviceClient.GetTwinAsync();
            var t = dt.Properties.Reported;
            return JsonConvert.DeserializeObject<JObject>(t[Id].ToString());
        }

        public string InvokeCommand(string command, string input)
        {
            CommandDef def = Commands.FirstOrDefault(x => x.Command == command);
            if (def == null)
            {
                throw new InvalidOperationException("command already exists");
            }

            return def.Handler(command, input);
        }

        public async Task SendEvent(string name, string value)
        {
            if (!Events.Contains(name))
            {
                throw new InvalidOperationException("Property doesn't exist");
            }

            var telemetryDataPoint = value;

            var messageString = JsonConvert.SerializeObject(telemetryDataPoint);
            var message = new Message(Encoding.ASCII.GetBytes(messageString));
            message.Properties.Add("interfaceid", Id);
            message.Properties.Add("eventname", name);
            await DeviceClient.SendEventAsync(message);
        }

        internal async Task UpdateProperty(string property, bool value)
        {
            if (!Properties.ContainsKey(property))
            {
                throw new InvalidOperationException("Property doesn't exist");
            }

            Properties[property] = value.ToString();

            TwinCollection settings = new TwinCollection();
            settings[Id] = GetJson();

            await DeviceClient.UpdateReportedPropertiesAsync(settings);
        }
    }
}
