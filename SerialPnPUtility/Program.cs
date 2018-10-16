using PnpGateway;
using PnpGateway.Serial;
using System;
using System.Threading.Tasks;

namespace SerialPnPUtility
{
    class Program
    {
        static void Main(string[] args)
        {
            SerialPnPDevice dev = null;
            PnpDeviceClient dclient = new PnpDeviceClient();

            while (true) {
                string command = Console.ReadLine();

                var cmd = command.Split(" ");

                if (cmd[0].Equals("open"))
                {
                    dev = new SerialPnPDevice(cmd[1], dclient);
                    dev.Start();
                }

                else if (cmd[0].Equals("list"))
                {
                    Console.WriteLine("Listing commands, interface " + dev.Interfaces[0].Id);
                    if (cmd[1].Equals("c")) {
                        foreach (var c in dev.Interfaces[0].Commands)
                        {
                            Console.WriteLine("COMMAND : " + c.Name);
                            Console.WriteLine("\t Display Name: " + c.DisplayName);
                            Console.WriteLine("\t Description : " + c.Description);
                            Console.WriteLine("\t Req Schema  : " + c.RequestSchema.ToString());
                            Console.WriteLine("\t Resp Schema : " + c.ResponseSchema.ToString());
                        }
                    }

                    else if (cmd[1].Equals("p"))
                    {
                        Console.WriteLine("Listing properties, interface " + dev.Interfaces[0].Id);
                        foreach (var c in dev.Interfaces[0].Properties)
                        {
                            Console.WriteLine("PROPERTY : " + c.Name);
                            Console.WriteLine("\t Display Name: " + c.DisplayName);
                            Console.WriteLine("\t Description : " + c.Description);
                            Console.WriteLine("\t Units       : " + c.Units);
                            Console.WriteLine("\t Schema      : " + c.DataSchema.ToString());
                        }
                    }

                    else if (cmd[1].Equals("e"))
                    {
                        Console.WriteLine("Listing events, interface " + dev.Interfaces[0].Id);
                        foreach (var c in dev.Interfaces[0].Events)
                        {
                            Console.WriteLine("EVENT : " + c.Name);
                            Console.WriteLine("\t Display Name: " + c.DisplayName);
                            Console.WriteLine("\t Description : " + c.Description);
                            Console.WriteLine("\t Units       : " + c.Units);
                            Console.WriteLine("\t Schema      : " + c.DataSchema.ToString());
                        }
                    }
                }

                else if (cmd[0].Equals("e"))
                {
                    dclient.iface.eventson = !dclient.iface.eventson;
                }

                else if (cmd[0].Equals("sp"))
                {
                    dclient.iface.propertyHandler(cmd[1], cmd[2]);
                }

                else if (cmd[0].Equals("xc")) {
                    Task.Run(async () =>
                    {
                        var res = await dclient.iface.methodHandler(cmd[1], cmd[2]);
                        Console.WriteLine("RET : " + res);
                    });
                }
            }
        }
    }
}
