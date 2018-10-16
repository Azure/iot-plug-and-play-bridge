using PnpGateway;
using PnpGateway.Serial;
using System;

namespace SerialPnPUtility
{
    class Program
    {
        static void Main(string[] args)
        {
            SerialPnPDevice dev = null;
            PnpDeviceClient dclient = null;

            while (true) {
                string command = Console.ReadLine();

                var cmd = command.Split(" ");

                if (cmd[0].Equals("open"))
                {
                    dev = new SerialPnPDevice(cmd[1], dclient);
                    dev.Start();
                }

            }
        }
    }
}
