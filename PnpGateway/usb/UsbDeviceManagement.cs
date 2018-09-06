using Microsoft.Azure.Devices.Client;
using Microsoft.Azure.Devices.Shared;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Newtonsoft.Json;
using System.Text;
using System.Globalization;
using System.Linq;

namespace PnpGateway
{
    class UsbDeviceManagement
    {
        public UsbDeviceManagement(DeviceClient deviceClient)
        {
            m_DeviceClient = deviceClient;
            m_DeviceClient.SetMethodHandlerAsync("enable", EnableDevice, null).Wait();
            m_DeviceClient.SetMethodHandlerAsync("disable", DisableDevice, null).Wait();
            m_DiscoveredDevices = new Dictionary<string, GenericWinUsbIoInterface>();
        }

        private Dictionary<string, GenericWinUsbIoInterface> m_DiscoveredDevices;

        public Task<MethodResponse> EnableDevice(MethodRequest methodRequest, object userContext)
        {
            UInt32 configRet = 0;
            UInt32 devInst = 0;

            if (m_DiscoveredDevices.Count <= 0)
            {
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes("No device"), 400));
            }

            var data = Encoding.UTF8.GetString(methodRequest.Data);
            int index = Int32.Parse(data);

            configRet = CfgMgr.CM_Locate_DevNodeW(ref devInst, m_DiscoveredDevices.Values.ToArray()[index].DeviceInstanceId, 0);
            if (configRet != CfgMgr.CR_SUCCESS)
            {
                Console.WriteLine("CM_Locate_DevNodeW failed: " + configRet);
                goto end;
            }
            //disable device using CM API
            configRet = CfgMgr.CM_Enable_DevNode(devInst, 0);
            if (configRet != CfgMgr.CR_SUCCESS)
            {
                Console.WriteLine("CM_Enable_DevNode failed: " + configRet);
            }

        end:
            // return result response
            string result = "{\"result\":\"" + configRet + "\"}";
            return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 200));
        }

        public Task<MethodResponse> DisableDevice(MethodRequest methodRequest, object userContext)
        {
            UInt32 configRet = 0;
            UInt32 devInst = 0;


            if (m_DiscoveredDevices.Count <= 0)
            {
                return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes("No device"), 400));
            }

            var data = Encoding.UTF8.GetString(methodRequest.Data);
            int index = Int32.Parse(data);

            configRet = CfgMgr.CM_Locate_DevNodeW(ref devInst, m_DiscoveredDevices.Values.ToArray()[index].DeviceInstanceId, 0);
            if (configRet != CfgMgr.CR_SUCCESS)
            {
                Console.WriteLine("CM_Locate_DevNodeW failed: " + configRet);
                goto end;
            }
  
            //disable device using CM API
            configRet = CfgMgr.CM_Disable_DevNode(devInst, CfgMgr.CM_DISABLE_PERSIST);
            if (configRet != CfgMgr.CR_SUCCESS)
            {
                Console.WriteLine("CM_Disable_DevNode failed: " + configRet);
            }

        end:
            // return result response
            string result = "{\"result\":\"" + configRet + "\"}";
            return Task.FromResult(new MethodResponse(Encoding.UTF8.GetBytes(result), 200));
        }

        DeviceClient m_DeviceClient;
        UIntPtr m_NotifyContext;

        private async void OnUsbDeviceConnect(
            string deviceInterfaceSymbolicLinkName,
            Guid deviceInterfaceGuid)
        {
            var symLinkName = deviceInterfaceSymbolicLinkName.ToUpper(CultureInfo.InvariantCulture);

            GenericWinUsbIoInterface device;
            Dictionary<string, GenericWinUsbIoInterface> discoveredDevices;
            var winUsbDevices = new WinUsbDevices(deviceInterfaceGuid);
            var result = winUsbDevices.DiscoverUsbDevices(out discoveredDevices);
            string json = null;

            if (result == true && discoveredDevices.ContainsKey(symLinkName))
            {
                device = discoveredDevices[symLinkName];
                if (!m_DiscoveredDevices.ContainsKey(deviceInterfaceSymbolicLinkName)) {
                    m_DiscoveredDevices.Add(deviceInterfaceSymbolicLinkName, device);
                }
            }

            TwinCollection settings = new TwinCollection();
            List<UsbPnpInterface> deviceNames = m_DiscoveredDevices.Values.Select(x => new UsbPnpInterface(x)).ToList();
            json = JsonConvert.SerializeObject(deviceNames);
            settings["IoT-PnP-UsbDeviceManagement"] = json;
            await m_DeviceClient.UpdateReportedPropertiesAsync(settings);
        }

        class UsbPnpInterface
        {
            public UsbPnpInterface(GenericWinUsbIoInterface device)
            {
                FriendlyName = device.FriendlyName;
                DeviceInterface = device.DeviceInstanceId;
            }

            public string FriendlyName { get; set; }

            public string DeviceInterface { get; set; }
        }

        private async void OnUsbDeviceDisconnect(string deviceInterfaceSymbolicLinkName)
        {
            if (m_DiscoveredDevices.Keys.Contains(deviceInterfaceSymbolicLinkName)) {
               // m_DiscoveredDevices.Remove(deviceInterfaceSymbolicLinkName);
                List<UsbPnpInterface> deviceNames = m_DiscoveredDevices.Values.Select(x => new UsbPnpInterface(x)).ToList();
                var json = JsonConvert.SerializeObject(deviceNames);

                TwinCollection settings = new TwinCollection();
                settings["IoT-PnP-UsbDeviceManagement"] = json;
                await m_DeviceClient.UpdateReportedPropertiesAsync(settings);
            }
        }

        public static Guid GUID_DEVINTERFACE_USB_DEVICE = new Guid("A5DCBF10-6530-11D2-901F-00C04FB951ED");

        internal unsafe uint UsbNotificationCallback(
            IntPtr hNotify,
            IntPtr Context,
            CfgMgr.CM_NOTIFY_ACTION Action,
            ref CfgMgr.CM_NOTIFY_EVENT_DATA EventData,
            uint EventDataSize
            )
        {
            string symLink;
            switch (Action)
            {
                case CfgMgr.CM_NOTIFY_ACTION.CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL:
                    fixed (ushort* symLinkPtr = EventData.u.DeviceInterface.SymbolicLink)
                    {
                        symLink = Marshal.PtrToStringUni(new IntPtr(symLinkPtr));
                    }

                    OnUsbDeviceConnect(symLink, EventData.u.DeviceInterface.ClassGuid);
                    break;

                case CfgMgr.CM_NOTIFY_ACTION.CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL:
                    fixed (ushort* symLinkPtr = EventData.u.DeviceInterface.SymbolicLink)
                    {
                        symLink = Marshal.PtrToStringUni(new IntPtr(symLinkPtr));
                    }

                    OnUsbDeviceDisconnect(symLink);
                    break;
            }

            return (uint)CfgMgr.WinError.Success;
        }

        public async void StartMonitoring(
           // Guid DeviceInterfaceGuid
            )
        {
            var json = JsonConvert.SerializeObject(m_DiscoveredDevices.Keys.ToList());

            TwinCollection settings = new TwinCollection();
            settings["IoT-PnP-UsbDeviceManagement"] = json;
            await m_DeviceClient.UpdateReportedPropertiesAsync(settings);

            //WinUsbDevices.DiscoverUsbDevices(DeviceInterfaceGuid);
            var filter = new CfgMgr.CM_NOTIFY_FILTER();
                filter.cbSize = (uint)Marshal.SizeOf(filter);
                filter.FilterType = CfgMgr.CM_NOTIFY_FILTER_TYPE.CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
                filter.Flags = 0;
                filter.Reserved = 0;
                filter.u.DeviceHandle.hTarget = IntPtr.Zero;
                filter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_USB_DEVICE;

                var result = CfgMgr.CM_Register_Notification(filter,
                                                             IntPtr.Zero,
                                                             UsbNotificationCallback,
                                                             out m_NotifyContext);

                if (result != 0)
                {
                    throw new Exception("Unable to register for USB connection" +
                                        " and disconnection notifications.");
                }
        }
    }
}
