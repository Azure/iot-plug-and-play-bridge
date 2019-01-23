using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PnpGateway.Serial
{
    public enum Schema
    {
        Invalid = 0,
        Byte,
        Float,
        Double,
        Int,
        Long,
        Boolean,
        String
    }

    public class InterfaceDefinition
    {
        public string Id = "";
        public int Index = 0;
        public List<EventDefinition> Events = new List<EventDefinition>();
        public List<PropertyDefinition> Properties = new List<PropertyDefinition>();
        public List<CommandDefinition> Commands = new List<CommandDefinition>();
    }

    public class EventDefinition : FieldDefinition
    {
        public Schema DataSchema;
        public string Units;
    }

    public class PropertyDefinition : FieldDefinition
    {
        public string Units;
        public bool Required;
        public bool Writeable;
        public Schema DataSchema;
    }

    public class CommandDefinition : FieldDefinition
    {
        public Schema RequestSchema;
        public Schema ResponseSchema;
    }

    public class FieldDefinition
    {
        public string Name;
        public string DisplayName;
        public string Description;
    }

    class SerialPnPDevice
    {
        private static Schema SchemaFromShort(ushort prequest_schema)
        {
            return (Schema)prequest_schema;
        }

        private static string BinarySchemaToString(Schema Schema, byte[] Data)
        {
            var rxstrdata = "???";

            if ((Schema == Schema.Float) && (Data.Length == 4))
            {
                var fa = new float[1];
                Buffer.BlockCopy(Data, 0, fa, 0, 4);
                rxstrdata = fa[0].ToString();
            }
            else if (((Schema == Schema.Int) && (Data.Length == 4)))
            {
                var fa = new int[1];
                Buffer.BlockCopy(Data, 0, fa, 0, 4);
                rxstrdata = fa[0].ToString();
            }

            return rxstrdata;
        }

        private static byte[] StringSchemaToBinary(Schema Schema, string Data)
        {
            var bd = new byte[0];

            if ((Schema == Schema.Float) || (Schema == Schema.Int))
            {
                bd = new byte[4];

                if (Schema == Schema.Float)
                {
                    var x = new float[1];
                    x[0] = float.Parse(Data);
                    Buffer.BlockCopy(x, 0, bd, 0, 4);
                } else if (Schema == Schema.Int)
                {
                    var x = new int[1];
                    x[0] = int.Parse(Data);
                    Buffer.BlockCopy(x, 0, bd, 0, 4);
                }
            } else if (Schema == Schema.Boolean)
            {
                bd = new byte[1];
                bool bdd = bool.Parse(Data);

                bd[0] = (byte) (bdd ? 1 : 0);
            }

            return bd;
        }

        private SerialPnPPacketInterface PacketInterface = null;
        private PnpDeviceClient DeviceClient = null;

        // Discard incoming data until operational is set
        private bool Operational = false;

        public string DisplayName;
        public List<InterfaceDefinition> Interfaces;
        private PnPInterface PnpInterface = null;

        public SerialPnPDevice(String ComPort, PnpDeviceClient DeviceClient)
        {
            this.PacketInterface = new SerialPnPPacketInterface(ComPort);
            this.DeviceClient = DeviceClient;
            this.PacketInterface.PacketReceieved += UnsolicitedPacket;

            if (DeviceClient != null)
            {
                this.PnpInterface = new PnPInterface("pnpSerial_" + ComPort, this.DeviceClient, PropertyHandler, MethodHandler);
            }
        }

        // This could potentially be moved out into a different function
        // but calling it seperately for now so init can occur asynchronously.
        public async Task Start()
        {
            Console.WriteLine("Opening serial port");
            this.PacketInterface.Open();

            // Reset the device
            await DeviceReset();

            // Get the descriptor
            var descriptor = await DeviceDescriptorRequest();

            // Parse the descriptor
            ParseDescriptor(descriptor);

            // Register with device client
            if (this.DeviceClient != null)
            {
                await RegisterWithPnP();
            }

            // Now query all properties to get PnP the updated value
            //foreach (var property in this.Interfaces[0].Properties)
            //{
            //    var rxPayload = await TxNameDataAndRxResponse(0, property.Name, 0x07, null);
            //    var stval = BinarySchemaToString(property.DataSchema, rxPayload);
            //    this.PnpInterface.WriteProperty(property.Name, stval);
            //}

            // Set operational now we understand the different data types
            this.Operational = true;
        }

        public void Stop()
        {
            this.PacketInterface.Close();
        }

        private async Task RegisterWithPnP()
        {
            var interf = this.Interfaces[0];

            foreach (var ee in interf.Commands)
            {
                this.PnpInterface.BindCommand(ee.Name);
            }

            foreach (var ee in interf.Events)
            {
                this.PnpInterface.BindEvent(ee.Name);
            }

            foreach (var ee in interf.Properties)
            {
                this.PnpInterface.BindProperty(ee.Name);
            }

            await this.DeviceClient.PublishInterface(PnpInterface);
        }

        private EventDefinition LookupEvent(string EventName, int InterfaceId)
        {
            foreach (var ev in this.Interfaces[InterfaceId].Events)
            {
                if (ev.Name.Equals(EventName))
                {
                    return ev;
                }
            }

            return null;
        }

        private CommandDefinition LookupMethod(string Name, int InterfaceId)
        {
            foreach (var ev in this.Interfaces[InterfaceId].Commands)
            {
                if (ev.Name.Equals(Name))
                {
                    return ev;
                }
            }

            return null;
        }

        private PropertyDefinition LookupProperty(string Name, int InterfaceId)
        {
            foreach (var ev in this.Interfaces[InterfaceId].Properties)
            {
                if (ev.Name.Equals(Name))
                {
                    return ev;
                }
            }

            return null;
        }

        // These are the device client entry points
        public async Task<string> MethodHandler(string command, string input)
        {
            var target = this.LookupMethod(command, 0);

            if (target == null)
            {
                return "false";
            }

            // otherwise serialize data
            var inputPayload = StringSchemaToBinary(target.RequestSchema, input);

            // execute method
            var rxPayload = await TxNameDataAndRxResponse(0, target.Name, 0x05, inputPayload);
            
            var stval = BinarySchemaToString(target.ResponseSchema, rxPayload);

            return stval;
        }

        public async Task PropertyHandler(string property, string input)
        {
            var target = this.LookupProperty(property, 0);

            if (target == null)
            {
                return;
            }

            // otherwise serialize data
            var inputPayload = StringSchemaToBinary(target.DataSchema, input);

            // execute method
            var rxPayload = await TxNameDataAndRxResponse(0, target.Name, 0x07, inputPayload);

            var stval = BinarySchemaToString(target.DataSchema, rxPayload);

            this.PnpInterface.WriteProperty(property, stval);
        }

        private async Task<byte[]> TxNameDataAndRxResponse(int InterfaceId, string Property, byte Type, byte[] RawValue)
        {
            int rvl = 0;
            if (RawValue != null)
            {
                rvl = RawValue.Length;
            }

            byte[] nameBytes = Encoding.UTF8.GetBytes(Property);
            byte[] txPacket = new byte[6 + nameBytes.Length + rvl];

            txPacket[0] = (byte) ((txPacket.Length) & 0xFF);
            txPacket[1] = (byte)((txPacket.Length) >> 8);
            txPacket[2] = Type; // property request
            // [3] is reserved
            txPacket[4] = (byte) InterfaceId;
            txPacket[5] = (byte) nameBytes.Length;

            Buffer.BlockCopy(nameBytes, 0, txPacket, 6, nameBytes.Length);
            if (RawValue != null)
            {
                Buffer.BlockCopy(RawValue, 0, txPacket, 6 + nameBytes.Length, RawValue.Length);
            }

            // Todo: if multiple properties are being set, we could potentially race here
            // if the response to another one / unsolicited one comes in.
            var rx = PacketInterface.RxPacket((byte) (Type + 1));

            PacketInterface.TxPacket(txPacket);
            await rx;
            var rxPacket = rx.Result;

            // Validate the field is correct
            var rxNameLength = rxPacket[5];
            var rxInterfaceId = rxPacket[4];
            var rxName = new byte[rxNameLength];
            Buffer.BlockCopy(rxPacket, 6, rxName, 0, rxNameLength);

            if ((rxNameLength != nameBytes.Length) ||
                (rxInterfaceId != InterfaceId) ||
                (!rxName.Equals(Property)))
            {
                this.UnsolicitedPacket(this, rxPacket);
            }

            byte[] rxPayload = new byte[rxPacket.Length - (6 + nameBytes.Length)];
            Buffer.BlockCopy(rxPacket, 6 + nameBytes.Length, rxPayload, 0, rxPayload.Length);

            return rxPayload;
        }

        private async Task<byte[]> DeviceDescriptorRequest()
        {
            // Prepare packet
            byte[] txPacket = new byte[4]; // packet header
            txPacket[0] = 4; // length 4
            txPacket[1] = 0;
            txPacket[2] = 0x03; // type descriptor request

            // Get ready to receieve a reset response
            var rx = PacketInterface.RxPacket(0x04);

            // Send the new packet
            PacketInterface.TxPacket(txPacket);
            Console.WriteLine("Sent descriptor request");

            await rx;
            var rxPacket = rx.Result;

            if (rxPacket[2] != 0x04)
            {
                throw new Exception("Bad descriptor response");
            }

            Console.WriteLine("Receieved descriptor response, of length " + rxPacket.Length.ToString());

            return rxPacket;
        }

        private async Task DeviceReset()
        {
            // Prepare packet
            byte[] resetPacket = new byte[4]; // packet header
            resetPacket[0] = 4; // length 4
            resetPacket[1] = 0;
            resetPacket[2] = 0x01; // type reset

            // Get ready to receieve a reset response
            var rx = PacketInterface.RxPacket(0x02);

            // Send the new packet
            PacketInterface.TxPacket(resetPacket);
            Console.WriteLine("Sent reset request");

            await rx;
            var responsePacket = rx.Result;

            if (responsePacket[2] != 0x02)
            {
                throw new Exception("Bad reset response");
            }

            Console.WriteLine("Receieved reset response");
        }

        private void UnsolicitedPacket(object sender, byte[] packet)
        {
            if (!Operational)
            {
                Console.WriteLine("Dropped packet due to non-operational state.");
                return; // drop the packet if we're not operational
            }

            // Got an event
            if (packet[2] == 0x0A) {
                var rxNameLength = packet[5];
                var rxInterfaceId = packet[4];
                var rxDataSize = packet.Length - rxNameLength - 6;

                string event_name = Encoding.UTF8.GetString(packet, 6, rxNameLength);

                var ev = this.LookupEvent(event_name, rxInterfaceId);

                var rxData = new byte[rxDataSize];
                Buffer.BlockCopy(packet, 6 + rxNameLength, rxData, 0, rxDataSize);

                var rxstrdata = BinarySchemaToString(ev.DataSchema, rxData);

                Console.WriteLine("-> Event " + event_name + " with data size " + rxDataSize + " schema " + ev.DataSchema.ToString () + " " + rxstrdata);

                if (DeviceClient != null)
                {
                    this.PnpInterface.SendEvent(event_name, rxstrdata);
                }
            }

            // Got a property update
            else if (packet[2] == 0x08)
            {
                // TODO
            }
        }
 
        private void ParseDescriptor(byte[] descriptor)
        {
            int c = 4;

            ushort version = descriptor[c++];
            ushort display_name_length = descriptor[c++];

            string display_name = Encoding.UTF8.GetString(descriptor, c, display_name_length);

            Console.WriteLine("Device Version : " + version.ToString());
            Console.WriteLine("Device Name    : " + display_name);

            this.DisplayName = display_name;
            this.Interfaces = new List<InterfaceDefinition>();

            c += display_name_length;

            while (c < descriptor.Length)
            {
                if (descriptor[c++] == 0x05)
                {
                    // inline interface
                    var indef = new InterfaceDefinition();

                    // parse ID
                    ushort interface_id_length = (ushort) (descriptor[c++] | (descriptor[c++] << 8));
                    string interface_id = Encoding.UTF8.GetString(descriptor, c, interface_id_length);
                    c += interface_id_length;

                    Console.WriteLine("Interface ID : " + interface_id);

                    indef.Id = interface_id;

                    // now process the different types
                    while (c < descriptor.Length)
                    {
                        if (descriptor[c] > 0x03) break; // not in a type of nested entry anymore

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

                            ushort prequest_schema = (ushort) (descriptor[c++] | (descriptor[c++] << 8));
                            ushort presponse_schema = (ushort) (descriptor[c++] | (descriptor[c++] << 8));

                            tfdef.RequestSchema = SchemaFromShort(prequest_schema);
                            tfdef.ResponseSchema = SchemaFromShort(presponse_schema);

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

                            ushort schema = (ushort)(descriptor[c++] | (descriptor[c++] << 8));
                            tfdef.DataSchema = SchemaFromShort(schema);

                            byte flags = descriptor[c++];

                            bool prequired = (flags & (1 << 1)) != 0;
                            bool pwriteable = (flags & (1 << 0)) != 0;

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

                            ushort schema = (ushort)(descriptor[c++] | (descriptor[c++] << 8));
                            tfdef.DataSchema = SchemaFromShort(schema);

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
