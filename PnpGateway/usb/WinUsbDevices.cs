using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;

namespace PnpGateway
{
    class WinUsbDevices
    {
        #region Enumerations

        /// <summary>
        /// This enumeration lists the status of uninstalling USB devices.
        /// </summary>
        public enum UninstallStatus
        {
            /// <summary>
            /// This status indicates that all the devices were uninstalled
            /// successfully.
            /// </summary>
            StatusOk,

            /// <summary>
            /// This status indicates that all the device were uninstalled
            /// successfully and the system needs to be rebooted for the 
            /// uninstallation to be complete.
            /// </summary>
            RebootNeeded,

            /// <summary>
            /// This status indicates that device uinstallation failed because
            /// access to the devices was denied. This can happen when the 
            /// device uninstallation was attempted from a non-administrator
            /// environment.
            /// </summary>
            AccessDenied,

            /// <summary>
            /// This status indicates that the device uninstallation failed
            /// because the device uininstallation was attempted from a 32-bit
            /// context on a 64-bit system.
            /// </summary>
            ErrorInWow64,

            /// <summary>
            /// This status indicates that the device uninstallation encountered
            /// an unspecified failure.
            /// </summary>
            GeneralFailure
        }

        #endregion

        public Guid DeviceInterfaceGuid { get; }

        #region Constructor

        /// <summary>
        /// This is the constructor for this class.
        /// </summary>
        /// <param name="deviceInterfaceGuid">
        /// Supplies the GUID for the device interface that should be supported
        /// by the devices.
        /// </param>
        public WinUsbDevices(
            Guid deviceInterfaceGuid
            )
        {
            DeviceInterfaceGuid = deviceInterfaceGuid;
        }

        #endregion

        #region Public

        /// <summary>
        /// This routine discovers USB devices connected to the host which 
        /// support the specified device interface as defined by a GUID.
        /// </summary>
        /// <param name="Devices">
        /// Supplies a reference to a dictionary that is used to return the 
        /// devices discovered by this routine. The key for the dictionary 
        /// elements is the symbolic link name for the device interface of the 
        /// device and the value is a GenericWinUsbIoInterface object 
        /// representing the device.
        /// </param>
        /// <returns>
        /// Returns true if the discovery of devices succeeded and false 
        /// otherwise.
        /// </returns>
        public unsafe bool DiscoverUsbDevices(
            out Dictionary<string, GenericWinUsbIoInterface> Devices
            )
        {
            IntPtr deviceInfoSet;

            Devices = new Dictionary<string, GenericWinUsbIoInterface>();
            var result = GetDeviceInformationSet(DeviceInterfaceGuid,
                                                 out deviceInfoSet);

            if (result == false)
            {
                goto Exit;
            }

            int deviceIndex = 0;
            do
            {
                DeviceInterfaceDetailData* deviceInterfaceDetailData;
                DeviceInformationData deviceInfoData;
                result = GetDeviceInformation(DeviceInterfaceGuid,
                                              deviceInfoSet,
                                              deviceIndex,
                                              out deviceInterfaceDetailData,
                                              out deviceInfoData);


                //
                // If the result is false, return immediately. If the result is
                // true but no information about the device is returned it means
                // that the device with the specified device index doesn't
                // exist i.e. there are no more devices to discover and the 
                // routine should return right away.
                //

                if ((result == false) || (deviceInfoData.Size == 0))
                {
                    goto Exit;
                }

                string deviceInterfaceSymbolicLinkName;
                result = GetDeviceInterfaceSymbolicLinkName(deviceInterfaceDetailData,
                                                            out deviceInterfaceSymbolicLinkName);

                if (result == false)
                {
                    goto Exit;
                }

                Marshal.FreeHGlobal((IntPtr)deviceInterfaceDetailData);

                var friendlyName = GetDeviceFriendlyName(deviceInfoSet,
                                                         ref deviceInfoData);

                //
                // Get the device instance ID for this device.
                //

                string deviceInstanceId;
                result = GetDeviceInstanceId(deviceInfoSet,
                                             deviceInfoData,
                                             out deviceInstanceId);
                if (result == false)
                {
                    goto Exit;
                }


                var device = new GenericWinUsbIoInterface(deviceInstanceId,
                                                          friendlyName,
                                                          deviceInterfaceSymbolicLinkName,
                                                          DeviceInterfaceGuid);

                var symLinkName = deviceInterfaceSymbolicLinkName.ToUpper(CultureInfo.InvariantCulture);
                Devices.Add(symLinkName, device);
                deviceIndex += 1;

            } while (true);

            Exit:
            return result;
        }

        /// <summary>
        /// This routine uninstalls all USB devices that support a specific
        /// device interface as defined by the device interface GUID.
        /// </summary>
        /// <param name="uninstalledDevices">
        /// Supplies a reference to a variable that is used to return the list 
        /// of uninstalled devices.
        /// </param>
        /// <returns>
        /// Returns status codes depending on the status of the uninstallation.
        /// </returns>
        /// <exception cref="Win32Exception">
        /// Throws a Win32Exception with an WIN32 error code that is usually 
        /// returned by the GetLastError Win32 function.
        /// </exception>
        /// <exception cref="Exception">
        /// Throws an exception if the symbolic link name for the device
        /// interface for the device is found to be null.
        /// </exception>
        public unsafe UninstallStatus UninstallDevices(
            out List<GenericWinUsbIoInterface> uninstalledDevices
            )
        {

            UninstallStatus retVal = UninstallStatus.StatusOk;
            IntPtr deviceInfoSet;
            bool rebootNeeded = false;
            int deviceIndex = 0;

            uninstalledDevices = new List<GenericWinUsbIoInterface>();
            var result = GetDeviceInformationSet(DeviceInterfaceGuid,
                                                 out deviceInfoSet);

            if (result == false)
            {
                goto Exit;
            }

            do
            {
                DeviceInterfaceDetailData* deviceInterfaceDetailData;
                DeviceInformationData deviceInfoData;
                result = GetDeviceInformation(DeviceInterfaceGuid,
                                              deviceInfoSet,
                                              deviceIndex,
                                              out deviceInterfaceDetailData,
                                              out deviceInfoData);


                //
                // If the result is false, return immediately. If the result is
                // true but no information about the device is returned it means
                // that the device with the specified device index doesn't
                // exist i.e. there are no more devices to discover and the 
                // routine should return right away.
                //

                if (result == false)
                {
                    retVal = UninstallStatus.GeneralFailure;
                    break;
                }

                if (deviceInfoData.Size == 0)
                {
                    retVal = UninstallStatus.StatusOk;
                    break;
                }

                string deviceInterfaceSymbolicLinkName;
                result = GetDeviceInterfaceSymbolicLinkName(deviceInterfaceDetailData,
                                                            out deviceInterfaceSymbolicLinkName);

                if (result == false)
                {
                    retVal = UninstallStatus.GeneralFailure;
                    break;
                }

                Marshal.FreeHGlobal((IntPtr)deviceInterfaceDetailData);

                var friendlyName = GetDeviceFriendlyName(deviceInfoSet,
                                                         ref deviceInfoData);

                //
                // Get the device instance ID for this device.
                //

                string deviceInstanceId;
                result = GetDeviceInstanceId(deviceInfoSet,
                                             deviceInfoData,
                                             out deviceInstanceId);
                if (result == false)
                {
                    retVal = UninstallStatus.GeneralFailure;
                    break;
                }

                //
                // Try to uninstall the device.
                //

                bool needsReboot;
                result = NativeMethods.DiUninstallDevice(IntPtr.Zero,
                                                         deviceInfoSet,
                                                         ref deviceInfoData,
                                                         0,
                                                         out needsReboot);

                if (needsReboot)
                {
                    rebootNeeded = true;
                }

                if (result == false)
                {
                    uint win32ErrorCode = (uint)Marshal.GetLastWin32Error();
                    if (win32ErrorCode == (uint)WinError.AccessDenied)
                    {
                        retVal = UninstallStatus.AccessDenied;
                        break;
                    }

                    if (win32ErrorCode == (uint)WinError.ErrorInWow64)
                    {
                        retVal = UninstallStatus.ErrorInWow64;
                        break;
                    }
                }

                var device = new GenericWinUsbIoInterface(deviceInstanceId,
                                                          friendlyName,
                                                          deviceInterfaceSymbolicLinkName,
                                                          DeviceInterfaceGuid);

                uninstalledDevices.Add(device);
                deviceIndex += 1;
            } while (true);

            Exit:
            if ((retVal == UninstallStatus.StatusOk) && rebootNeeded)
            {
                retVal = UninstallStatus.RebootNeeded;
            }

            return retVal;
        }

        #endregion

        #region Private

        private static unsafe string GetDeviceFriendlyName(
            IntPtr deviceInfoSet,
            ref DeviceInformationData deviceInfoData
            )
        {
            uint requiredSize;
            uint propertyRegDataType;
            DeviceRegistryProperties deviceRegistryProperties = DeviceRegistryProperties.SPDRP_FRIENDLYNAME;
            //DeviceRegistryProperties.SPDRP_DEVICEDESC };

            var result = NativeMethods.SetupDiGetDeviceRegistryProperty(
                deviceInfoSet,
                ref deviceInfoData,
                deviceRegistryProperties,
                out propertyRegDataType,
                (byte*)IntPtr.Zero,
                0,
                out requiredSize
                );

            if (result == false)
            {
                var errorCode = Marshal.GetLastWin32Error();
                if (errorCode != (int)WinError.InsufficientBuffer)
                {
                    //throw new Win32Exception(errorCode);
                    Console.WriteLine("Failed to get friendly name using SPDRP_FRIENDLYNAME " + errorCode);
                }
                deviceRegistryProperties = DeviceRegistryProperties.SPDRP_DEVICEDESC;

                result = NativeMethods.SetupDiGetDeviceRegistryProperty(
                       deviceInfoSet,
                       ref deviceInfoData,
                       deviceRegistryProperties,
                       out propertyRegDataType,
                       (byte*)IntPtr.Zero,
                       0,
                       out requiredSize
                       );

                if (result == false)
                {
                    errorCode = Marshal.GetLastWin32Error();
                    if (errorCode != (int)WinError.InsufficientBuffer)
                    {
                        Console.WriteLine("Failed to get friendly name using SPDRP_FRIENDLYNAME " + errorCode);
                        return "";
                    }
                }
            }

            byte[] buffer = new byte[requiredSize];
            fixed (byte* bufferPtr = buffer)
            {
                result = NativeMethods.SetupDiGetDeviceRegistryProperty(
                    deviceInfoSet,
                    ref deviceInfoData,
                    deviceRegistryProperties,
                    out propertyRegDataType,
                    bufferPtr,
                    requiredSize,
                    out requiredSize
                    );

                if (result == false)
                {
                    var errorCode = Marshal.GetLastWin32Error();
                    throw new Win32Exception(errorCode);
                }
            }

            //
            // Convert the friendly name from a byte buffer to a unicode
            // string and remove the trailing zero of a C-style string.
            //

            var friendlyName = Encoding.Unicode.GetString(buffer);
            friendlyName = friendlyName.Remove(friendlyName.Length - 1);
            return friendlyName;
        }

        private static unsafe bool GetDeviceInterfaceSymbolicLinkName(
            DeviceInterfaceDetailData* deviceInterfaceDetailData,
            out string deviceinterfaceSymLinkName
            )
        {
            deviceinterfaceSymLinkName = Marshal.PtrToStringAuto(new IntPtr(&deviceInterfaceDetailData->DevicePath));
            if (deviceinterfaceSymLinkName == null)
            {
                return false;
            }

            return true;
        }

        private static unsafe bool GetDeviceInformation(
            Guid DeviceInterfaceGuid,
            IntPtr deviceInfoSet,
            int deviceIndex,
            out DeviceInterfaceDetailData* deviceInterfaceDetailData,
            out DeviceInformationData deviceInfoData
            )
        {
            var bufferSize = 0;
            var deviceInterfaceData = new DeviceInterfaceData()
            {
                Size = Marshal.SizeOf(typeof(DeviceInterfaceData))
            };

            deviceInterfaceDetailData = null;
            deviceInfoData = new DeviceInformationData();

            if (false == NativeMethods.SetupDiEnumDeviceInterfaces(
                deviceInfoSet,
                0,
                ref DeviceInterfaceGuid,
                deviceIndex,
                ref deviceInterfaceData))
            {
                var errorCode = Marshal.GetLastWin32Error();

                if (errorCode == (int)WinError.NoMoreItems)
                {
                    return true;
                }

                return false;
            }

            if (false == NativeMethods.SetupDiGetDeviceInterfaceDetail(
                deviceInfoSet,
                ref deviceInterfaceData,
                IntPtr.Zero,
                0,
                ref bufferSize,
                IntPtr.Zero))
            {
                var errorCode = Marshal.GetLastWin32Error();

                if (errorCode != (int)WinError.InsufficientBuffer)
                {
                    return false;
                }
            }

            deviceInterfaceDetailData = (DeviceInterfaceDetailData*)Marshal.AllocHGlobal(bufferSize);
            //headaches on 32/64 getting size to work...
            //pack, sizeof, etc. nothing works
            //harcoding

            if (IntPtr.Size == 4)
            {
                deviceInterfaceDetailData->Size = 6;
            }
            else
            {
                deviceInterfaceDetailData->Size = 8;
            }

            deviceInfoData = new DeviceInformationData()
            {
                Size = Marshal.SizeOf(typeof(DeviceInformationData))
            };

            if (false == NativeMethods.SetupDiGetDeviceInterfaceDetail(
                deviceInfoSet,
                ref deviceInterfaceData,
                deviceInterfaceDetailData,
                bufferSize,
                ref bufferSize,
                ref deviceInfoData))
            {
                return false;
            }

            return true;
        }

        private static bool GetDeviceInformationSet(
            Guid DeviceInterfaceGuid,
            out IntPtr deviceInfoSet)
        {
            deviceInfoSet = NativeMethods.SetupDiGetClassDevs(
                ref DeviceInterfaceGuid,
                null,
                0,
                (int)(DIGCF.Present | DIGCF.DeviceInterface));

            if (IntPtr.Zero == deviceInfoSet)
            {
                return false;
            }

            return true;
        }

        private static unsafe bool GetDeviceInstanceId(
            IntPtr deviceInfoSet,
            DeviceInformationData deviceInfoData,
            out string deviceInstanceId
            )
        {

            uint requiredSize;

            deviceInstanceId = null;
            var result = NativeMethods.SetupDiGetDeviceInstanceId(
                deviceInfoSet,
                ref deviceInfoData,
                IntPtr.Zero,
                0,
                out requiredSize);

            if (result == false)
            {
                var errorCode = Marshal.GetLastWin32Error();
                if (errorCode != (int)WinError.InsufficientBuffer)
                {
                    goto Exit;
                }
            }

            //
            // 2-bytes per Unicode character.
            //

            var buffer = new byte[requiredSize * 2];
            fixed (byte* bufferPtr = buffer)
            {
                result = NativeMethods.SetupDiGetDeviceInstanceId(
                    deviceInfoSet,
                    ref deviceInfoData,
                    (IntPtr)bufferPtr,
                    requiredSize,
                    out requiredSize
                    );

                if (result == false)
                {
                    goto Exit;
                }
            }

            //
            // Convert the device instance id from a byte buffer to a unicode
            // string and remove the trailing zero of a C-style string.
            //

            deviceInstanceId = Encoding.Unicode.GetString(buffer);
            deviceInstanceId = deviceInstanceId.Remove(deviceInstanceId.Length - 1);

            Exit:
            return result;
        }

        #endregion
    }
}

