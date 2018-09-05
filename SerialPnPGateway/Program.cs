using System;

namespace SerialPnPGateway
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Hello World!");

            var a = new Device("COM4", null);

            while (true)
            {
                var s = Console.ReadLine();

                if (s.Length == 0) break;

                var pargs = s.Split(" ");

                if (pargs[0].Equals("gp"))
                {
                    a.GetProperty(pargs[1]);
                } else if (pargs[0].Equals("sp"))
                {
                    a.SetProperty(pargs[1], UInt16.Parse(pargs[2]));
                } else if (pargs[0].Equals("c"))
                {
                    a.SendCommand(pargs[1], byte.Parse(pargs[2]));
                }
            }
        }
    }
}
