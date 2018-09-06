using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace PnpGateway
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    struct SerialPnPPacketHeader
    {
        public byte StartOfFrame;
        public ushort Length;
        public byte PacketType;
    }

    public enum Schema
    {
        None = 0,
        Bytes,
        Bool,
        Float,
        Int
    }

    public class InterfaceDefinition
    {
        public string Id = "";
        public List<EventDefinition> Events = new List<EventDefinition>();
        public List<PropertyDefinition> Properties = new List<PropertyDefinition>();
        public List<CommandDefinition> Commands = new List<CommandDefinition>();
    }

    public class EventDefinition : FieldDefinition
    {
        public Schema Schema;
        public string Units;
    }

    public class PropertyDefinition : FieldDefinition
    {
        public string Units;
        public bool Required;
        public bool Writeable;
    }

    public class CommandDefinition : FieldDefinition
    {
        public Schema InputSchema;
        public Schema OutputSchema;
    }

    public class FieldDefinition
    {
        public string Name;
        public string DisplayName;
        public string Description;
    }

}