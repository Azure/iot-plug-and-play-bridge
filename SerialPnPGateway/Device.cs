using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Text;

namespace SerialPnPGateway
{
    public class DeviceEventArgs : EventArgs
    {
        public EventDefinition EventDefinition;
        public byte[] EventData;
    }

    public class Device
    {
        public event EventHandler<DeviceEventArgs> NewEvent;

        public string DisplayName;
        public List<InterfaceDefinition> Interfaces = new List<InterfaceDefinition>();

        byte[] rxbuffer = new byte[1024];
        int rxbufferindex = 0;
        bool escaped = false;

        SerialPort port;

        public Device(String ComPort)
        {
            port = new SerialPort(ComPort, 115200, Parity.None, 8, StopBits.One);

            port.DataReceived += RxCallback;
            port.Open();

            // send descriptor request
            SerialPnPPacketHeader dr = new SerialPnPPacketHeader();
            dr.StartOfFrame = 0x5A;
            dr.Length = (ushort)Marshal.SizeOf(dr);
            dr.PacketType = 1; // Get descriptor

            this.SendPacket(dr);
        }

        public void GetProperty(String Property)
        {
            byte[] propname = Encoding.UTF8.GetBytes(Property);

            SerialPnPPacketHeader dr = new SerialPnPPacketHeader();
            dr.StartOfFrame = 0x5A;
            dr.Length = (ushort) (Marshal.SizeOf(dr) + propname.Length + 1);
            dr.PacketType = 7; // Get property

            byte[] lnb = new byte[1];
            lnb[0] = (byte) propname.Length;

            this.SendPacket(dr);
            this.SendBytes(lnb);
            this.SendBytes(propname);

            Console.WriteLine("Sent request for property \"" + Property + "\"");
        }

        public void SetProperty(String Property, uint Value)
        {
            byte[] propname = Encoding.UTF8.GetBytes(Property);

            SerialPnPPacketHeader dr = new SerialPnPPacketHeader();
            dr.StartOfFrame = 0x5A;
            dr.Length = (ushort)(Marshal.SizeOf(dr) + propname.Length + 1 + 4);
            dr.PacketType = 5; // set property

            byte[] lnb = new byte[1];
            lnb[0] = (byte)propname.Length;

            byte[] val = BitConverter.GetBytes(Value);

            this.SendPacket(dr);
            this.SendBytes(lnb);
            this.SendBytes(propname);
            this.SendBytes(val);

            Console.WriteLine("Sent set for property \"" + Property + "\" : " + Value.ToString());
        }

        public void SendCommand(String Command, byte Argument)
        {
            byte[] propname = Encoding.UTF8.GetBytes(Command);

            SerialPnPPacketHeader dr = new SerialPnPPacketHeader();
            dr.StartOfFrame = 0x5A;
            dr.Length = (ushort)(Marshal.SizeOf(dr) + propname.Length + 1 + 1);
            dr.PacketType = 3; // command

            byte[] lnb = new byte[1];
            lnb[0] = (byte)propname.Length;

            byte[] val = new byte[1]; //BitConverter.GetBytes(Value);
            val[0] = Argument;

            this.SendPacket(dr);
            this.SendBytes(lnb);
            this.SendBytes(propname);
            this.SendBytes(val);

            Console.WriteLine("Sent command \"" + Command + "\" with parameter " + ((uint)Argument).ToString());
        }

        private void RxCallback(object sender, SerialDataReceivedEventArgs e)
        {
            int inb;

            while (port.BytesToRead > 0)
            {
                inb = port.ReadByte();

                if (inb == 0x5A)
                {
                    Console.Write("\n");
                }
                Console.Write("0x" + inb.ToString("X2") + " ");

                if (inb == 0xEF && !escaped)
                {
                    escaped = true;
                    continue;
                }
                else if (inb == 0x5A && !escaped)
                {
                    rxbufferindex = 0;
                }

                escaped = false;

                rxbuffer[rxbufferindex++] = (byte)inb;

                if (rxbufferindex >= 4)
                {
                    SerialPnPPacketHeader hdr = new SerialPnPPacketHeader();
                    IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf(hdr));

                    Marshal.Copy(rxbuffer, 0, ptr, Marshal.SizeOf(hdr));

                    hdr = (SerialPnPPacketHeader)Marshal.PtrToStructure(ptr, hdr.GetType());
                    Marshal.FreeHGlobal(ptr);

                    if (hdr.Length == rxbufferindex)
                    {
                        ProcessPacket(hdr);
                        rxbufferindex = 0;
                    }
                }

            }
        }

        private byte[] PacketToBytes(SerialPnPPacketHeader hdr)
        {
            int size = Marshal.SizeOf(hdr);
            byte[] arr = new byte[size];

            IntPtr ptr = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(hdr, ptr, true);
            Marshal.Copy(ptr, arr, 0, size);
            Marshal.FreeHGlobal(ptr);

            return arr;
        }

        private void SendBytes(byte[] barr)
        {
            int c = 0;
            int start = 0;
            byte[] tmp = { 0xEf };

            for (c = 0; c < barr.Length; c++)
            {
                if (barr[c] == 0x5a || barr[c] == 0xEF)
                {
                    if (c - start > 0)
                    {
                        port.Write(barr, start, c - start);
                        port.Write(tmp, 0, 1);
                    }

                    start = c;
                }
            }

            port.Write(barr, start, barr.Length - start);
        }

        private void SendPacket(SerialPnPPacketHeader hdr)
        {
            //hdr.Length = revshort(hdr.Length);

            byte[] pb = PacketToBytes(hdr);
            this.port.Write(pb, 0, 1); // send the 0x5A

            byte[] pba = new byte[pb.Length - 1];
            Array.Copy(pb, 1, pba, 0, pb.Length - 1);

            SendBytes(pba);
        }

        private void ProcessPacket(SerialPnPPacketHeader hdr)
        {
            System.Console.WriteLine("\nReceived packet type " + hdr.PacketType.ToString());

            byte[] payload = new byte[hdr.Length - Marshal.SizeOf(hdr)];
            Array.Copy(this.rxbuffer, Marshal.SizeOf(hdr), payload, 0, payload.Length);

            if (hdr.PacketType == 2)
            {
                ParseDescriptor(payload);
            }
            else if (hdr.PacketType == 0x0A)
            {
                // sensor data
                ushort name_length = payload[0];
                string name = Encoding.UTF8.GetString(payload, 1, name_length);

                byte[] data = new byte[payload.Length - (name_length + 1)];
                Array.Copy(payload, name_length + 1, data, 0, data.Length);

                // find matching event template
                EventDefinition evdef = null;
                foreach(var interf in this.Interfaces) {
                    foreach (var ev in interf.Events)
                    {
                        if (ev.Name.Equals(name)) {
                            evdef = ev;
                            break;
                        }
                    }
                }

                if (evdef != null)
                {
                    Console.Write("Got data \"" + name + "\" :");

                    if (evdef.Schema == Schema.Float)
                    {
                        Console.WriteLine("float " + BitConverter.ToSingle(data, 0));
                    }
                    else if (evdef.Schema == Schema.Int)
                    {
                        Console.WriteLine("int (u) " + BitConverter.ToUInt32(data, 0));
                    }
                    else
                    {
                        Console.WriteLine("unknown " + data.Length.ToString() + " bytes");
                    }


                    var arg = new DeviceEventArgs();
                    arg.EventData = data;
                    arg.EventDefinition = evdef;

                    var hnd = this.NewEvent;
                    if (hnd != null)
                    {
                        hnd(this, arg);
                    }
                }
            }

            // incoming property
            else if (hdr.PacketType == 8)
            {
                ushort name_length = payload[0];
                string name = Encoding.UTF8.GetString(payload, 1, name_length);

                byte[] data = new byte[payload.Length - (name_length + 1)];
                Array.Copy(payload, name_length + 1, data, 0, data.Length);

                uint val = BitConverter.ToUInt32(data, 0);

                Console.WriteLine("Got property \"" + name + "\" : [" + data.Length.ToString() + "] " + val.ToString());
            }

            // command response
            else if (hdr.PacketType == 4)
            {
                ushort name_length = payload[0];
                string name = Encoding.UTF8.GetString(payload, 1, name_length);

                byte[] data = new byte[payload.Length - (name_length + 1)];
                Array.Copy(payload, name_length + 1, data, 0, data.Length);

                uint val = (uint) data[0];

                Console.WriteLine("Got command response \"" + name + "\" : [" + data.Length.ToString() + "] " + val.ToString());
            }
        }

        private void ParseDescriptor(byte[] descriptor)
        {
            ushort version = descriptor[0];
            ushort display_name_length = descriptor[1];

            string display_name = Encoding.UTF8.GetString(descriptor, 2, display_name_length);

            Console.WriteLine("Slave version : " + version.ToString());
            Console.WriteLine("Slave display name : " + display_name);

            this.DisplayName = display_name;
            this.Interfaces = new List<InterfaceDefinition>();

            int c = 2 + display_name_length;

            while (c < descriptor.Length)
            {
                if (descriptor[c++] == 0x04)
                {
                    // inline interface
                    var indef = new InterfaceDefinition();

                    // parse ID
                    ushort interface_id_length = descriptor[c++];
                    string interface_id = Encoding.UTF8.GetString(descriptor, c, interface_id_length);
                    c += interface_id_length;

                    Console.WriteLine("Interface : " + interface_id);

                    indef.Id = interface_id;

                    // now process the different types
                    while (c < descriptor.Length)
                    {
                        if (descriptor[c] > 0x03) break; // not in a type of interface parameter anymore

                        FieldDefinition fielddef = null;

                        // extract common field properties
                        ushort ptype = descriptor[c++];

                        ushort pname_length = descriptor[c++];
                        string pname = Encoding.UTF8.GetString(descriptor, c, pname_length);
                        c += pname_length;

                        ushort pdisplay_name_length = descriptor[c++];
                        string pdisplay_name = Encoding.UTF8.GetString(descriptor, c, pdisplay_name_length);
                        c += pdisplay_name_length;

                        ushort pdescription_length = descriptor[c++];
                        string pdescription = (pdescription_length > 0) ? Encoding.UTF8.GetString(descriptor, c, pdescription_length) : "";
                        c += pdescription_length;

                        Console.WriteLine("\tProperty type : " + ptype.ToString());
                        Console.WriteLine("\tName : " + pname);
                        Console.WriteLine("\tDisplay Name : " + pdisplay_name);
                        Console.WriteLine("\tDescription : " + pdescription);

                        if (ptype == 0x01) // command
                        {
                            var tfdef = new CommandDefinition();
                            fielddef = tfdef;

                            byte prequest_schema = descriptor[c++];
                            byte presponse_schema = descriptor[c++];

                            tfdef.InputSchema = (Schema) prequest_schema;
                            tfdef.OutputSchema = (Schema)presponse_schema;

                            indef.Commands.Add(tfdef);
                        }
                        else if (ptype == 0x02) // property
                        {
                            var tfdef = new PropertyDefinition();
                            fielddef = tfdef;

                            ushort punit_length = descriptor[c++];
                            string punit = Encoding.UTF8.GetString(descriptor, c, punit_length);
                            c += punit_length;

                            Console.WriteLine("\tUnit : " + punit);

                            bool prequired = descriptor[c++] != 0;
                            bool pwriteable = descriptor[c++] != 0;

                            tfdef.Units = punit;
                            tfdef.Required = prequired;
                            tfdef.Writeable = pwriteable;

                            indef.Properties.Add(tfdef);
                        }
                        else if (ptype == 0x03) // event
                        {
                            var tfdef = new EventDefinition();
                            fielddef = tfdef;

                            ushort punit_length = descriptor[c++];
                            string punit = Encoding.UTF8.GetString(descriptor, c, punit_length);
                            c += punit_length;

                            Console.WriteLine("\tUnit : " + punit);

                            byte pschema = descriptor[c++];

                            tfdef.Schema = (Schema)pschema;
                            tfdef.Units = punit;

                            indef.Events.Add(tfdef);
                        }

                        fielddef.Name = pname;
                        fielddef.DisplayName = pdisplay_name;
                        fielddef.Description = pdescription;
                    }

                    this.Interfaces.Add(indef);
                }
                else
                {
                    Console.WriteLine("Unsupported descriptor");
                    throw new Exception();
                }
            }
        }
    }
}

