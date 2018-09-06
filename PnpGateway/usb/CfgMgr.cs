using System;
using System.Runtime.InteropServices;

namespace PnpGateway
{
    internal class CfgMgr
    {
        internal const int MAX_DEVICE_ID_LEN = 200;
        internal const int ANYSIZE_ARRAY = 1;

        /// <summary>
        /// Windows Error Code.
        /// </summary>
        internal enum WinError : uint
        {
            Success = 0,
            Cancelled = 1223,
        }

        [Flags]
        internal enum CM_NOTIFY_FILTER_FLAGS : uint
        {
            CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES = 0x00000001,
            CM_NOTIFY_FILTER_FLAG_ALL_DEVICE_INSTANCES = 0x00000002
        }

        internal enum CM_NOTIFY_FILTER_TYPE
        {

            CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE = 0,
            CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE,
            CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE,
            CM_NOTIFY_FILTER_TYPE_MAX
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct CM_NOTIFY_FILTER_DEVICE_INTERFACE
        {
            public Guid ClassGuid;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct CM_NOTIFY_FILTER_DEVICE_HANDLE
        {
            public IntPtr hTarget;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal unsafe struct CM_NOTIFY_FILTER_DEVICE_INSTANCE
        {
            public fixed ushort InstanceId[MAX_DEVICE_ID_LEN];
        }

        [StructLayout(LayoutKind.Explicit)]
        internal struct CM_NOTIFY_FILTER_UNION
        {
            [FieldOffset(0)]
            public CM_NOTIFY_FILTER_DEVICE_INTERFACE DeviceInterface;

            [FieldOffset(0)]
            public CM_NOTIFY_FILTER_DEVICE_HANDLE DeviceHandle;

            [FieldOffset(0)]
            public CM_NOTIFY_FILTER_DEVICE_INSTANCE DeviceInstance;
        };

        [StructLayout(LayoutKind.Sequential)]
        internal class CM_NOTIFY_FILTER
        {
            public uint cbSize;
            public CM_NOTIFY_FILTER_FLAGS Flags;
            public CM_NOTIFY_FILTER_TYPE FilterType;
            public uint Reserved;
            public CM_NOTIFY_FILTER_UNION u;
        }

        internal enum CM_NOTIFY_ACTION
        {
            /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE */

            CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL = 0,
            CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL,

            /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE */

            CM_NOTIFY_ACTION_DEVICEQUERYREMOVE,
            CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED,
            CM_NOTIFY_ACTION_DEVICEREMOVEPENDING,
            CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE,
            CM_NOTIFY_ACTION_DEVICECUSTOMEVENT,

            /* Filter type: CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE */

            CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED,
            CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED,
            CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED,

            CM_NOTIFY_ACTION_MAX
        }

        [StructLayout(LayoutKind.Sequential)]
        internal unsafe struct CM_NOTIFY_EVENT_DATA_DEVICE_INTERFACE
        {
            public Guid ClassGuid;
            public fixed ushort SymbolicLink[ANYSIZE_ARRAY];
        };

        [StructLayout(LayoutKind.Sequential)]
        internal unsafe struct CM_NOTIFY_EVENT_DATA_DEVICE_HANDLE
        {
            public Guid EventGuid;
            public int NameOffset;
            public uint DataSize;
            public fixed byte Data[ANYSIZE_ARRAY];
        }

        [StructLayout(LayoutKind.Sequential)]
        internal unsafe struct CM_NOTIFY_EVENT_DATA_DEVICE_INSTANCE
        {
            public fixed ushort InstanceId[ANYSIZE_ARRAY];
        }

        [StructLayout(LayoutKind.Explicit)]
        internal struct CM_NOTIFY_EVENT_DATA_UNION
        {
            [FieldOffset(0)]
            public CM_NOTIFY_EVENT_DATA_DEVICE_INTERFACE DeviceInterface;

            [FieldOffset(0)]
            public CM_NOTIFY_EVENT_DATA_DEVICE_HANDLE DeviceHandle;

            [FieldOffset(0)]
            public CM_NOTIFY_EVENT_DATA_DEVICE_INSTANCE DeviceInstance;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct CM_NOTIFY_EVENT_DATA
        {

            public CM_NOTIFY_FILTER_TYPE FilterType;
            public uint Reserved;
            public CM_NOTIFY_EVENT_DATA_UNION u;
        }

        internal delegate uint CM_NOTIFY_CALLBACK(
            IntPtr hNotify,
            IntPtr Context,
            CM_NOTIFY_ACTION Action,
            ref CM_NOTIFY_EVENT_DATA EventData,
            uint EventDataSize
            );

        [DllImport("CfgMgr32.dll", SetLastError = true, EntryPoint = "CM_Register_Notification")]
        [return: MarshalAs(UnmanagedType.U4)]
        internal static extern uint CM_Register_Notification(
            [In, MarshalAs(UnmanagedType.LPStruct)]
            CM_NOTIFY_FILTER pFilter,
            IntPtr pContext,
            CM_NOTIFY_CALLBACK pCallback,
            out UIntPtr pNotifyContext
            );

        [DllImport("CfgMgr32.dll", SetLastError = true, EntryPoint = "CM_Unregister_Notification")]
        [return: MarshalAs(UnmanagedType.U4)]
        internal static extern uint CM_Unregister_Notification(
            UIntPtr NotifyContext
            );

        [DllImport("cfgmgr32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.U4)]
        public static extern UInt32 CM_Locate_DevNodeW(ref UInt32 DevInst, string DeviceID, UInt32 Flags);


        public const UInt32 CR_SUCCESS = 0;
        public const UInt32 DN_NEED_RESTART = 0x00000100;
        public const UInt32 CM_PROB_NEED_RESTART = 0x0000000E;
        public const UInt32 CM_PROB_DISABLED = 0x00000016;
        public const UInt32 CM_DISABLE_PERSIST = 0x00000008;
        public const UInt32 CONFIGFLAG_DISABLED = 0x00000001;

        [DllImport("cfgmgr32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.U4)]
        public static extern UInt32 CM_Enable_DevNode(UInt32 DevInst, UInt32 Flags);

        [DllImport("cfgmgr32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.U4)]
        public static extern UInt32 CM_Disable_DevNode(UInt32 DevInst, UInt32 Flags);
    }
}
