using System;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.Win32.SafeHandles;

//WinUsbSetupPacket defined below [Potential duplicate]

namespace PnpGateway
{
    #region Enums

    /// <summary>
    /// USB Request type.
    /// </summary>
    [Flags]
    public enum UsbRequest
    {
        /// <summary>
        /// Specifies that the direction of the data flow for this USB request 
        /// is from the device to the host.
        /// </summary>
        DeviceToHost = 0x80,

        /// <summary>
        /// Specifies that the direction of the data flow for this USB request
        /// is from the host to the device.
        /// </summary>
        HostToDevice = 0x00,

        /// <summary>
        /// Specifies that the USB request is a standard request that all 
        /// devices must support.
        /// </summary>
        Standard = 0x00,

        /// <summary>
        /// Specifies that the USB request is for a specific class of USB
        /// devices.
        /// </summary>
        Class = 0x20,

        /// <summary>
        /// Specifies that this is a vendor-defined USB request.
        /// </summary>
        Vendor = 0x40,

        /// <summary>
        /// This value is Reserved.
        /// </summary>
        Reserved = 0x60,

        /// <summary>
        /// Specifies that the USB request is directed to the entire device.
        /// </summary>
        ForDevice = 0x00,

        /// <summary>
        /// Specifies that the USB request is directed towards a specific
        /// interface on the device.
        /// </summary>
        ForInterface = 0x01,

        /// <summary>
        /// Specifies that the USB request is directed towards a specific 
        /// endpoint on the device.
        /// </summary>
        ForEndpoint = 0x02,

        /// <summary>
        /// Specifies that the USB request is not directed towards either the 
        /// entire device, a specific interface or a specific endpoint.
        /// </summary>
        ForOther = 0x03
    }

    /// <summary>
    /// WinUsb Pipe type.
    /// </summary>
    internal enum WinUsbPipeType
    {
        Control,

        Isochronous,

        Bulk,

        Interrupt,
    }

    /// <summary>
    /// USB Policy Type.
    /// </summary>
    internal enum WinUsbPolicyType : uint
    {
        ShortPacketTerminate = 1,

        AutoClearStall,

        PipeTransferTimeout,

        IgnoreShortPackets,

        AllowPartialReads,

        AutoFlush,

        RawIO,

        MaximumTransferSize,

        ResetPipeOnResume
    }

    /// <summary>
    /// File Access Rights.
    /// </summary>
    [Flags]
    internal enum AccessRights : uint
    {
        Read = (0x80000000),

        Write = (0x40000000),

        Execute = (0x20000000),

        All = (0x10000000)
    }

    /// <summary>
    /// File Share Mode
    /// </summary>
    [Flags]
    internal enum ShareModes : uint
    {
        Read = 0x00000001,

        Write = 0x00000002,

        Delete = 0x00000004
    }

    /// <summary>
    /// Create File Disposition.
    /// </summary>
    internal enum CreateFileDisposition : uint
    {
        CreateNew = 1,

        CreateAlways = 2,

        CreateExisting = 3,

        OpenAlways = 4,

        TruncateExisting = 5
    }

    /// <summary>
    /// File Flags.
    /// </summary>
    internal enum FileFlags : uint
    {
        Overlapped = 0x40000000
    }

    /// <summary>
    /// Windows Error Code.
    /// </summary>
    internal enum WinError : uint
    {
        Success = 0,

        FileNotFound = 2,

        PathNotFound = 3,

        AccessDenied = 5,

        InvalidData = 13,

        NoMoreFiles = 18,

        NotReady = 21,

        GeneralFailure = 31,

        InvalidParameter = 87,

        SemaphoreTimeout = 121,

        InsufficientBuffer = 122,

        AlreadyExists = 183,

        NoMoreItems = 259,

        IoPending = 997,

        DeviceNotConnected = 1167,

        ErrorInWow64 = 0xe0000235,

        TimeZoneIdInvalid = UInt32.MaxValue,

        InvalidHandleValue = UInt32.MaxValue

    }

    /// <summary>
    /// Flags controlling what is included in the device information set built by SetupDiGetClassDevs
    /// </summary>
    [Flags]
    internal enum DIGCF
    {
        Default = 0x00000001,

        Present = 0x00000002,

        AllClasses = 0x00000004,

        Profile = 0x00000008,

        DeviceInterface = 0x00000010
    }

    /// <summary>
    /// Properties that can be retrieved using the SetupDiGetDeviceRegistryProperty function.
    /// </summary>
    internal enum DeviceRegistryProperties : uint
    {
        SPDRP_DEVICEDESC = 0x00000000,
        SPDRP_FRIENDLYNAME = 0x0000000C
    }

    internal enum UsbDescriptorType : byte
    {
        Device = 0x01,

        Configuration = 0x02,

        String = 0x03,

        Interface = 0x04,

        Endpoint = 0x05,

        DeviceQualifier = 0x06,

        OtherSpeedConfiguration = 0x07,

        InterfacePower = 0x08,

        Otg = 0x09,

        Debug = 0x0a,

        InterfaceAssociation = 0x0b,

        Bos = 0x0f,

        DeviceCapability = 0x10,

        SuperspeedEndpointCompanion = 0x30
    }

    /// <summary>
    /// This enumeration defines the language IDs defined in 
    /// http://www.usb.org/developers/docs/USB_LANGIDs.pdf. These IDs are used
    /// by the USB string descriptors.
    /// </summary>
    internal enum UsbLanguageId : ushort
    {
        Afrikaans = 0x0436,

        Albanian = 0x041c,

        ArabicSaudiArabia = 0x0401,

        ArabicIraq = 0x0801,

        ArabicEgypt = 0x0c01,

        ArabicLibya = 0x1001,

        ArabicAlgeria = 0x1401,

        ArabicMorocco = 0x1801,

        ArabicTunisia = 0x1c01,

        ArabicOman = 0x2001,

        ArabicYemen = 0x2401,

        ArabicSyria = 0x2801,

        ArabicJordan = 0x2c01,

        ArabicLebanon = 0x3001,

        ArabicKuwait = 0x3401,

        ArabicUAE = 0x3801,

        ArabicBahrain = 0x3c01,

        ArabicQatar = 0x4001,

        Armenian = 0x042b,

        Assamese = 0x044d,

        AzeriLatin = 0x042c,

        AzeriCyrillic = 0x082c,

        Basque = 0x042d,

        Belarussian = 0x0423,

        Bengali = 0x0445,

        Bulgarian = 0x0402,

        Burmese = 0x0455,

        Catalan = 0x0403,

        ChineseTaiwan = 0x0404,

        ChinesePRC = 0x0804,

        ChineseHongKong = 0x0c04,

        ChineseSingapore = 0x1004,

        ChineseMacau = 0x1404,

        Croatian = 0x041a,

        Czech = 0x0405,

        Danish = 0x0406,

        DutchNetherlands = 0x0413,

        DutchBelgium = 0x0813,

        EnglishUnitedStates = 0x0409,

        EnglishUnitedKingdom = 0x0809,

        EnglishAustralian = 0x0c09,

        EnglishCanadian = 0x1009,

        EnglishNewZealand = 0x1409,

        EnglishIreland = 0x1809,

        EnglishSouthAfrica = 0x1c09,

        EnglishJamaica = 0x2009,

        EnglishCaribbean = 0x2409,

        EnglishBelize = 0x2809,

        EnglishTrinidad = 0x2c09,

        EnglishZimbabwe = 0x3009,

        EnglishPhilippines = 0x3409,

        Estonian = 0x0425,

        Faeroese = 0x0438,

        Farsi = 0x0429,

        Finnish = 0x040b,

        FrenchStandard = 0x040c,

        FrenchBelgian = 0x080c,

        FrenchCanadian = 0x0c0c,

        FrenchSwitzerland = 0x100c,

        FrenchLuxembourg = 0x140c,

        FrenchMonaco = 0x180c,

        Georgian = 0x0437,

        GermanStandard = 0x0407,

        GermanSwitzerland = 0x0807,

        GermanAustria = 0x0c07,

        GermanLuxembourg = 0x1007,

        GermanLiechtenstein = 0x1407,

        Greek = 0x0408,

        Gujarati = 0x0447,

        Hebrew = 0x040d,

        Hindi = 0x0439,

        Hungarian = 0x040e,

        Icelandic = 0x040f,

        Indonesian = 0x0421,

        ItalianStandard = 0x0410,

        ItalianSwitzerland = 0x0810,

        Japanese = 0x0411,

        Kannada = 0x044b,

        KashmiriIndia = 0x0860,

        Kazakh = 0x043f,

        Konkani = 0x0457,

        Korean = 0x0412,

        KoreanJohab = 0x0812,

        Latvian = 0x0426,

        Lithuanian = 0x0427,

        LithuanianClassic = 0x0827,

        Macedonian = 0x042f,

        MalayMalaysian = 0x043e,

        MalayBruneiDarussalam = 0x083e,

        Malayalam = 0x044c,

        Manipuri = 0x0458,

        Marathi = 0x044e,

        NepaliIndia = 0x0861,

        NorwegianBokmal = 0x0414,

        NorwegianNynorsk = 0x0814,

        Oriya = 0x0448,

        Polish = 0x0415,

        PortugueseBrazil = 0x0416,

        PortugueseStandard = 0x0816,

        Punjabi = 0x0446,

        Romanian = 0x0418,

        Russian = 0x0419,

        Sanskrit = 0x044f,

        SerbianCyrillic = 0x0c1a,

        SerbianLatin = 0x081a,

        Sindhi = 0x0459,

        Slovak = 0x041b,

        Slovenian = 0x0424,

        SpanishTraditionalSort = 0x040a,

        SpanishMexican = 0x080a,

        SpanishModernSort = 0x0c0a,

        SpanishGuatemala = 0x100a,

        SpanishCostaRica = 0x140a,

        SpanishPanama = 0x180a,

        SpanishDominicanRepublic = 0x1c0a,

        SpanishVenezuela = 0x200a,

        SpanishColombia = 0x240a,

        SpanishPeru = 0x280a,

        SpanishArgentina = 0x2c0a,

        SpanishEcuador = 0x300a,

        SpanishChile = 0x340a,

        SpanishUruguay = 0x380a,

        SpanishParaguay = 0x3c0a,

        SpanishBolivia = 0x400a,

        SpanishElSalvador = 0x440a,

        SpanishHonduras = 0x480a,

        SpanishNicaragua = 0x4c0a,

        SpanishPuertoRico = 0x500a,

        Sutu = 0x0430,

        SwahiliKenya = 0x0441,

        Swedish = 0x041d,

        SwedishFinland = 0x081d,

        Tamil = 0x0449,

        TatarTatarstan = 0x0444,

        Telugu = 0x044a,

        Thai = 0x041e,

        Turkish = 0x041f,

        Ukrainian = 0x0422,

        UrduPakistan = 0x0420,

        UrduIndia = 0x0820,

        UzbekLatin = 0x0443,

        UzbekCyrillic = 0x0843
    }

    #endregion

    #region Structs

    /// <summary>
    /// WinUsb Interface Descriptor.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct WinUsbInterfaceDescriptor
    {
        public byte Length;

        public byte DescriptorType;

        public byte InterfaceNumber;

        public byte AlternateSetting;

        public byte NumEndpoints;

        public byte InterfaceClass;

        public byte InterfaceSubClass;

        public byte InterfaceProtocol;

        public byte Interface;
    }

    /// <summary>
    /// WinUsb Pipe Information.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct WinUsbPipeInformation
    {
        public WinUsbPipeType PipeType;

        public byte PipeId;

        public ushort MaximumPacketSize;

        public byte Interval;
    }

    /// <summary>
    /// device Interface data.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct DeviceInterfaceData
    {
        public int Size;

        public Guid InterfaceClassGuid;

        public int Flags;

        public IntPtr Reserved;
    }

    /// <summary>
    /// device Information Data.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct DeviceInformationData
    {
        public int Size;

        public Guid ClassGuid;

        public int DevInst;

        public IntPtr Reserved;
    }

    /// <summary>
    /// device Interface Detail Data.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Unicode)]
    internal struct DeviceInterfaceDetailData
    {
        public int Size;

        public char DevicePath;
    }

    #endregion

    /// <summary>
    /// Class defining Native P/Invoke.
    /// </summary>
    internal static class NativeMethods
    {
        /// <summary>
        /// The WinUsb_Initialize routine retrieves a handle for the interface that is associated 
        /// with the indicated device.
        /// </summary>
        /// <param name="deviceHandle">
        /// The handle to the device that CreateFile returned. WinUSB uses overlapped I/O, so 
        /// FILE_FLAG_OVERLAPPED must be specified in the flagsAndAttributes parameter of 
        /// CreateFile call for deviceHandle to have the characteristics necessary for WinUsb_Initialize 
        /// to function properly.
        /// </param>
        /// <param name="interfaceHandle">
        /// The interface handle that WinUsb_Initialize returns. All other WinUSB routines require
        /// this handle as input. The handle is opaque.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_Initialize")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool WinUsbInitialize(
            SafeFileHandle deviceHandle,
            ref IntPtr interfaceHandle);

        /// <summary>
        /// The WinUsb_Free function frees all of the resources that WinUsb_Initialize allocated.
        /// </summary>
        /// <param name="interfaceHandle">The interface handle that WinUsb_Initialize returned.</param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_Free")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool WinUsbFree(
            IntPtr interfaceHandle);

        /// <summary>
        /// The WinUsb_QueryInterfaceSettings function returns the interface descriptor
        /// for the specified alternate interface settings for a particular interface handle.
        /// </summary>
        /// <param name="interfaceHandle">
        /// The interface handle that WinUsb_Initialize returned.
        /// </param>
        /// <param name="alternateInterfaceNumber">
        /// A value that indicates which alternate settings to return. A value of 0 indicates the first
        /// alternate setting, a value of 1 indicates the second alternate setting, and so on.
        /// </param>
        /// <param name="usbAltInterfaceDescriptor">
        /// A pointer to a caller-allocated USB_INTERFACE_DESCRIPTOR structure that contains information 
        /// about the interface that AlternateSettingNumber specified.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_QueryInterfaceSettings")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern Boolean WinUsbQueryInterfaceSettings(
            IntPtr interfaceHandle,
            byte alternateInterfaceNumber,
            ref WinUsbInterfaceDescriptor usbAltInterfaceDescriptor);

        /// <summary>
        /// The WinUsb_QueryPipe routine returns information about a pipe that is associated with 
        /// an interface.
        /// </summary>
        /// <param name="interfaceHandle">The interface handle that WinUsb_Initialize returned.</param>
        /// <param name="alternateInterfaceNumber">
        /// A value that specifies the alternate interface to return the information for.
        /// </param>
        /// <param name="pipeIndex">A value that specifies the pipe to return information about. This
        /// value is not the same as the bEndpointAddress field in the endpoint descriptor. A pipeIndex 
        /// value of 0 signifies the first endpoint that is associated with the interface, a value of 1 
        /// signifies the second endpoint, and so on. pipeIndex must be less than the value in the 
        /// bNumEndpoints field of the interface descriptor.
        /// </param>
        /// <param name="pipeInformation">Pipe Information.</param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_QueryPipe")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern Boolean WinUsbQueryPipe(
            IntPtr interfaceHandle,
            byte alternateInterfaceNumber,
            byte pipeIndex,
            ref WinUsbPipeInformation pipeInformation);

        /// <summary>
        /// The WinUsb_SetPipePolicy function sets the policy for a specific pipe (endpoint).
        /// </summary>
        /// <param name="interfaceHandle">The interface handle that WinUsb_Initialize returned.</param>
        /// <param name="pipeID">
        /// An 8-bit value that consists of a 7-bit address and a direction bit. This parameter 
        /// corresponds to the bEndpointAddress field in the endpoint descriptor.
        /// </param>
        /// <param name="policyType">
        /// A value that specifies the policy parameter to change. 
        /// </param>
        /// <param name="valueLength">
        /// The size, in bytes, of the buffer at Value.
        /// </param>
        /// <param name="value">
        /// The new value for the policy parameter that PolicyType specifies. The size of this input parameter
        /// depends on the policy to change. For information about the size of this parameter, see the description
        /// of the PolicyType parameter.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_SetPipePolicy")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern Boolean WinUsbSetPipePolicy(
            IntPtr interfaceHandle,
            byte pipeID,
            uint policyType,
            uint valueLength,
            ref bool value);

        /// <summary>
        /// The WinUsb_SetPipePolicy function sets the policy for a specific pipe (endpoint).
        /// </summary>
        /// <param name="interfaceHandle">The interface handle that WinUsb_Initialize returned.</param>
        /// <param name="pipeID">
        /// An 8-bit value that consists of a 7-bit address and a direction bit. This parameter 
        /// corresponds to the bEndpointAddress field in the endpoint descriptor.
        /// </param>
        /// <param name="policyType">
        /// A value that specifies the policy parameter to change. 
        /// </param>
        /// <param name="valueLength">
        /// The size, in bytes, of the buffer at Value.
        /// </param>
        /// <param name="value">
        /// The new value for the policy parameter that PolicyType specifies. The size of this input parameter
        /// depends on the policy to change. For information about the size of this parameter, see the description
        /// of the PolicyType parameter.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_SetPipePolicy")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern Boolean WinUsbSetPipePolicy(
            IntPtr interfaceHandle,
            byte pipeID,
            uint policyType,
            uint valueLength,
            ref uint value);

        /// <summary>
        /// The WinUsb_ReadPipe function reads data from a pipe.
        /// </summary>
        /// <param name="interfaceHandle">The interface handle that WinUsb_Initialize returned.</param>
        /// <param name="pipeID">
        /// An 8-bit value that consists of a 7-bit address and a direction bit. This parameter corresponds
        /// to the bEndpointAddress field in the endpoint descriptor.
        /// </param>
        /// <param name="buffer">
        /// A caller-allocated buffer that receives the data that is read.
        /// </param>
        /// <param name="bufferLength">
        /// The maximum number of bytes to read. This number must be less than or equal to the size,
        /// in bytes, of Buffer.
        /// </param>
        /// <param name="lengthTransferred">
        /// Amount data transferred.
        /// </param>
        /// <param name="overlapped">
        /// Overlapped structure for asynchronous operations.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_ReadPipe")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe Boolean WinUsbReadPipe(
            IntPtr interfaceHandle,
            byte pipeID,
            byte* buffer,
            uint bufferLength,
            out uint lengthTransferred,
            NativeOverlapped* overlapped);

        /// <summary>
        /// The WinUsb_WritePipe function writes data to a pipe.
        /// </summary>
        /// <param name="interfaceHandle">
        /// The interface handle that WinUsb_Initialize returned.
        /// </param>
        /// <param name="pipeID">
        /// An 8-bit value that consists of a 7-bit address and a direction bit. 
        /// This parameter corresponds to the bEndpointAddress field in the 
        /// endpoint descriptor.
        /// </param>
        /// <param name="buffer">
        /// A caller-allocated buffer that contains the data to write.
        /// </param>
        /// <param name="bufferLength">
        /// The number of bytes to write. This number must be less than or equal
        /// to the size, in bytes, of Buffer.
        /// </param>
        /// <param name="lengthTransferred">
        /// Amount data transferred.
        /// </param>
        /// <param name="overlapped">
        /// Overlapped structure for asynchronous operations.
        /// </param>
        /// <returns>
        /// true on success, false on failure.
        /// </returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_WritePipe")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe Boolean WinUsbWritePipe(
            IntPtr interfaceHandle,
            byte pipeID,
            byte* buffer,
            uint bufferLength,
            out uint lengthTransferred,
            NativeOverlapped* overlapped);


        /// <summary>
        /// The WinUsb_WritePipe function writes data to a pipe.
        /// </summary>
        /// <param name="interfaceHandle">
        /// The interface handle that WinUsb_Initialize returned.
        /// </param>
        /// <param name="setupPacket">
        /// The 8-byte setup packet.
        /// </param>
        /// <param name="buffer">
        /// A caller-allocated buffer that contains the data to transfer.
        /// </param>
        /// <param name="bufferLength">
        /// The number of bytes to transfer, not including the setup packet. 
        /// This number must be less than or equal to the size, in byte of the
        /// buffer. 
        /// <paramref name="buffer"/>.
        /// A caller-allocated buffer that contains the data to transfer. 
        /// The length of this buffer must not exceed 4KB.
        /// </param>
        /// <param name="lengthTransferred">
        /// Supplies a buffer that receives the actual number of transferred 
        /// bytes. 
        /// </param>
        /// <param name="overlapped">
        /// Overlapped structure for asynchronous operations.
        /// </param>
        /// <returns>
        /// true on success, false on failure.
        /// </returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_ControlTransfer")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe bool WinUsbControlTransfer(
            IntPtr interfaceHandle,
            WinUsbSetupPacket setupPacket,
            byte* buffer,
            uint bufferLength,
            out uint lengthTransferred,
            NativeOverlapped* overlapped);

        /// <summary>
        /// The WinUsb_WritePipe function writes data to a pipe.
        /// </summary>
        /// <param name="interfaceHandle">
        /// The interface handle that WinUsb_Initialize returned.
        /// </param>
        /// <param name="descriptorType">
        /// A value that specifies the type of descriptor to return.
        /// </param>
        /// <param name="index">
        /// The descriptor index. For an explanation of the descriptor index, 
        /// see the Universal Serial Bus specification (www.usb.org).
        /// </param>
        /// <param name="languageId">
        /// A value that specifies the language identifier, if the requested 
        /// descriptor is a string descriptor.
        /// </param>
        /// <param name="buffer">
        /// A caller-allocated buffer that receives the requested descriptor.
        /// </param>
        /// <param name="bufferLength">
        /// The length, in bytes, of Buffer.
        /// </param>
        /// <param name="lengthTransferred">
        /// Supplies a buffer that receives the actual number of transferred 
        /// bytes. 
        /// </param>
        /// <returns>
        /// true on success, false on failure.
        /// </returns>
        [DllImport("winusb.dll", SetLastError = true, EntryPoint = "WinUsb_GetDescriptor")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe bool WinUsbGetDescriptor(
            IntPtr interfaceHandle,
            byte descriptorType,
            byte index,
            ushort languageId,
            byte* buffer,
            uint bufferLength,
            out uint lengthTransferred);

        /// <summary>
        /// The SetupDiGetClassDevs function returns a handle to a device information set that contains 
        /// requested device information elements for a local computer. 
        /// </summary>
        /// <param name="classGuid">
        /// A pointer to the GUID for a device setup class or a device interface class. This pointer is 
        /// optional and can be NULL. For more information about how to set ClassGuid, see the following Comments section.
        /// </param>
        /// <param name="enumerator">
        /// Enumerator id.
        /// </param>
        /// <param name="parent">
        /// Handle to top level parent window.
        /// </param>
        /// <param name="flags">
        /// Control options that filter the device information elements that are added to the device information set. 
        /// </param>
        /// <returns>
        /// InvalidHandleValue on failure. Handle to device infoset on success
        /// </returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiGetClassDevs")]
        internal static extern IntPtr SetupDiGetClassDevs(
            ref Guid classGuid,
            String enumerator,
            int parent,
            int flags);

        /// <summary>
        /// The SetupDiEnumDeviceInterfaces function enumerates the device interfaces that are contained in a device 
        /// information set. 
        /// </summary>
        /// <param name="deviceInfoSet">
        /// A pointer to a device information set that contains the device interfaces for which to return information.
        /// This handle is typically returned by SetupDiGetClassDevs.
        /// </param>
        /// <param name="deviceInfoData">
        /// device info data.
        /// </param>
        /// <param name="interfaceClassGuid">
        /// A GUID that specifies the device interface class for the requested interface.
        /// </param>
        /// <param name="memberIndex">
        /// A zero-based index into the list of interfaces in the device information set
        /// </param>
        /// <param name="deviceInterfaceData">
        /// device interface data.
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiEnumDeviceInterfaces")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool SetupDiEnumDeviceInterfaces(
            IntPtr deviceInfoSet,
            int deviceInfoData,
            ref Guid interfaceClassGuid,
            int memberIndex,
            ref DeviceInterfaceData deviceInterfaceData);

        /// <summary>
        /// The SetupDiGetDeviceInterfaceDetail function returns details about a device interface.
        /// </summary>
        /// <param name="deviceInfoSet">
        /// Handle to the device information set that contains the interface for which to retrieve details. This handle is 
        /// typically returned by SetupDiGetClassDevs.
        /// </param>
        /// <param name="deviceInterfaceData">
        /// DeviceInterfaceData structure that specifies the interface in DeviceInfoSet for which to retrieve details.
        /// </param>
        /// <param name="deviceInterfaceDetailData">
        /// Pointer to device interface detail data.
        /// </param>
        /// <param name="deviceInterfaceDetailDataSize">
        /// Size of the device interface detail structure.
        /// </param>
        /// <param name="requiredSize">
        /// The required size of device interface detail data.
        /// </param>
        /// <param name="deviceInfoData">
        /// A pointer to a buffer that receives information about the device that supports the requested interface. 
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiGetDeviceInterfaceDetail")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool SetupDiGetDeviceInterfaceDetail(
            IntPtr deviceInfoSet,
            ref DeviceInterfaceData deviceInterfaceData,
            IntPtr deviceInterfaceDetailData,
            int deviceInterfaceDetailDataSize,
            ref int requiredSize,
            IntPtr deviceInfoData);

        /// <summary>
        /// The SetupDiGetDeviceInterfaceDetail function returns details about a device interface.
        /// </summary>
        /// <param name="deviceInfoSet">
        /// Handle to the device information set that contains the interface for which to retrieve details. This handle is 
        /// typically returned by SetupDiGetClassDevs.
        /// </param>
        /// <param name="deviceInterfaceData">
        /// DeviceInterfaceData structure that specifies the interface in DeviceInfoSet for which to retrieve details.
        /// </param>
        /// <param name="deviceInterfaceDetailData">
        /// Pointer to device interface detail data.
        /// </param>
        /// <param name="deviceInterfaceDetailDataSize">
        /// Size of the device interface detail structure.
        /// </param>
        /// <param name="requiredSize">
        /// The required size of device interface detail data.
        /// </param>
        /// <param name="deviceInfoData">
        /// A pointer to a buffer that receives information about the device that supports the requested interface. 
        /// </param>
        /// <returns>true on success, false on failure.</returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiGetDeviceInterfaceDetail")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe bool SetupDiGetDeviceInterfaceDetail(
            IntPtr deviceInfoSet,
            ref DeviceInterfaceData deviceInterfaceData,
            DeviceInterfaceDetailData* deviceInterfaceDetailData,
            int deviceInterfaceDetailDataSize,
            ref int requiredSize,
            ref DeviceInformationData deviceInfoData);

        /// <summary>
        /// The SetupDiGetDeviceRegistryProperty function retrieves a specified Plug and Play device property.
        ///  </summary>
        /// <param name="deviceInfoSet">
        /// Handle to the device information set that contains the interface for which to retrieve details. This handle is 
        /// typically returned by SetupDiGetClassDevs.
        /// </param>
        /// <param name="deviceInfoData">
        ///  A pointer to a buffer that receives information about the device that supports the requested interface. 
        ///  </param>
        /// <param name="property">
        /// One of the Plug and Play device properties to be retrieved. Please refer to MSDN for documentation.
        /// </param>
        /// <param name="propertyRegDataType">
        /// A pointer to a variable that receives the data type of the property that is being retrieved. This is one of the 
        /// standard registry data types. This parameter is optional and can be NULL.
        /// </param>
        /// <param name="propertyBuffer">
        /// A pointer to a buffer that receives the property that is being retrieved. If this parameter is set to NULL, 
        /// and PropertyBufferSize is also set to zero, the function returns the required size for the buffer in RequiredSize.
        /// </param>
        /// <param name="propertyBufferSize">
        /// The size, in bytes, of the PropertyBuffer buffer.
        /// </param>
        /// <param name="requiredSize">
        /// A pointer to a variable of type DWORD that receives the required size, in bytes, of the PropertyBuffer buffer 
        /// that is required to hold the data for the requested property. This parameter is optional and can be NULL.
        /// </param>
        /// <returns>true on sucess, false on failure</returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiGetDeviceRegistryProperty")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern unsafe bool SetupDiGetDeviceRegistryProperty(
              IntPtr deviceInfoSet,
              ref DeviceInformationData deviceInfoData,
              DeviceRegistryProperties property,
              out uint propertyRegDataType,
              byte* propertyBuffer,
              uint propertyBufferSize,
              out uint requiredSize);

        /// <summary>
        /// The SetupDiGetDeviceInstanceId function retrieves the device instance ID that is associated with a device information element.
        /// </summary>
        /// <param name="deviceInfoSet">
        /// A handle to the device information set that contains the device information element that represents the device for which to retrieve a device instance ID.
        /// </param>
        /// <param name="deviceInfoData">
        /// A pointer to an SP_DEVINFO_DATA structure that specifies the device information element in DeviceInfoSet.
        /// </param>
        /// <param name="deviceInstanceId">
        /// A pointer to the character buffer that will receive the NULL-terminated device instance ID for the specified device information element.
        /// </param>
        /// <param name="deviceInstanceIdSize">
        /// The size, in characters, of the DeviceInstanceId buffer.
        /// </param>
        /// <param name="requiredSize">
        /// A pointer to the variable that receives the number of characters required to store the device instance ID.
        /// </param>
        /// <returns>
        /// Returns TRUE if it is successful. Otherwise, it returns FALSE and the logged error can be retrieved by making a call to GetLastError.
        /// </returns>
        [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "SetupDiGetDeviceInstanceId")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool SetupDiGetDeviceInstanceId(
            IntPtr deviceInfoSet,
            ref DeviceInformationData deviceInfoData,
            IntPtr deviceInstanceId,
            uint deviceInstanceIdSize,
            out uint requiredSize);

        /// <summary>
        /// The DiUninstallDevice function uninstalls a device and removes its device node (devnode) from the system. 
        /// </summary>
        /// <param name="hwdParent">
        /// A handle to the top-level window that is used to display any user interface component that is associated with the uninstallation request for the device. This parameter is optional and can be set to NULL.
        /// </param>
        /// <param name="deviceInfoSet">
        /// A handle to the device information set that contains a device information element. This element represents the device to be uninstalled through this call.
        /// </param>
        /// <param name="deviceInfoData">
        /// A pointer to an SP_DEVINFO_DATA structure that represents the specified device in the specified device information set for which the uninstallation request is performed.
        /// </param>
        /// <param name="flags">
        /// A value of type DWORD that specifies device uninstallation flags. Starting with Windows 7, this parameter must be set to zero.
        /// </param>
        /// <param name="needReboot">
        /// A pointer to a value of type BOOL that DiUninstallDevice sets to indicate whether a system restart is required to complete the device uninstallation request. 
        /// </param>
        /// <returns>
        /// Returns TRUE if the function successfully uninstalled the top-level device node that represents the device. 
        /// </returns>
        [DllImport("newdev.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "DiUninstallDevice")]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool DiUninstallDevice(
            IntPtr hwdParent,
            IntPtr deviceInfoSet,
            ref DeviceInformationData deviceInfoData,
            uint flags,
            out bool needReboot);


        /// <summary>
        /// The CreateFile function creates or opens a file, file stream, directory, physical disk, volume, console buffer, tape drive,
        /// communications resource, mailslot, or named pipe. The function returns a handle that can be used to access an object.
        /// </summary>
        /// <param name="fileName"></param>
        /// <param name="desiredAccess"> access to the object, which can be read, write, or both</param>
        /// <param name="shareMode">The sharing mode of an object, which can be read, write, both, or none</param>
        /// <param name="securityAttributes">A pointer to a SECURITY_ATTRIBUTES structure that determines whether or not the returned handle can 
        /// be inherited by child processes. Can be null</param>
        /// <param name="creationDisposition">An action to take on files that exist and do not exist</param>
        /// <param name="flagsAndAttributes">The file attributes and flags. </param>
        /// <param name="templateFileHandle">A handle to a template file with the GENERIC_READ access right. The template file supplies file attributes 
        /// and extended attributes for the file that is being created. This parameter can be null</param>
        /// <returns>If the function succeeds, the return value is an open handle to a specified file. If a specified file exists before the function 
        /// all and dwCreationDisposition is CREATE_ALWAYS or OPEN_ALWAYS, a call to GetLastError returns ERROR_ALREADY_EXISTS, even when the function 
        /// succeeds. If a file does not exist before the call, GetLastError returns 0 (zero).
        /// If the function fails, the return value is INVALID_HANDLE_VALUE. To get extended error information, call GetLastError.
        /// </returns>
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "CreateFile")]
        internal static extern SafeFileHandle CreateFile(
            string fileName,
            uint desiredAccess,
            uint shareMode,
            IntPtr securityAttributes,
            uint creationDisposition,
            uint flagsAndAttributes,
            IntPtr templateFileHandle);
    }

    /// <summary>
    /// The <see cref="DniDeviceIoLibrary.Native"/> namespace contains classes 
    /// that allow native OS methods to be called from managed code.
    /// </summary>
    [System.Runtime.CompilerServices.CompilerGenerated]
    class NamespaceDoc
    {
    }
}
