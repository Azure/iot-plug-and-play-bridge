using System;
using System.Threading.Tasks;

namespace PnpGateway
{
    public class PnpDeviceClient
    {
        public PnPInterface iface = null;

        public async Task PublishInterface(PnPInterface pnpInterface)
        {
            iface = pnpInterface;
        }
    }

    public class PnPInterface
    {
        public string Id;
        public PnpDeviceClient deviceClient;
        public Func<string, string, Task> propertyHandler;
        public Func<string, string, Task<string>> methodHandler;

        public PnPInterface(string v, PnpDeviceClient deviceClient, Func<string, string, Task> propertyHandler, Func<string, string, Task<string>> methodHandler)
        {
            this.Id = v;
            this.deviceClient = deviceClient;
            this.propertyHandler = propertyHandler;
            this.methodHandler = methodHandler;
        }

        internal void WriteProperty(string name, string stval)
        {
            throw new NotImplementedException();
        }

        internal void BindCommand(string name)
        {
            throw new NotImplementedException();
        }

        internal void BindEvent(string name)
        {
            throw new NotImplementedException();
        }

        internal void BindProperty(string name)
        {
            throw new NotImplementedException();
        }

        internal void SendEvent(string event_name, string rxstrdata)
        {
            Console.WriteLine("Got event " + event_name + " : " + rxstrdata);
        }
    }
}