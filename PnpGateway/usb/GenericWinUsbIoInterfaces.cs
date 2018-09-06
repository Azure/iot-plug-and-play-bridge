using System;
using System.Collections.Generic;
using static PnpGateway.NativeMethods;

namespace PnpGateway
{
    /// <summary>
    /// This class provides an IO interface to a WinUsb device.
    /// </summary>
    public class GenericWinUsbIoInterface : IWinUsbIoInterface
    {
        #region Constructors

        /// <summary>
        /// This constructor creates an object that defines information for a
        /// particular USB device.
        /// </summary>
        /// <param name="DeviceInstanceId">
        /// Supplies the device instance ID for the USB device. The
        /// device instance ID is also referred to as the device instance path.
        /// This value shows up as device Interface Path in the device Manager.
        /// </param>
        /// <param name="FriendlyName">
        /// Supplies the friendly name reported by the USB device. 
        /// This value shows up as Friendly Name in the device Manager.</param>
        /// <param name="DeviceInterfaceSymbolicLinkName">
        /// Supplies the symbolic link name for one of the device's interfaces. 
        /// This value can be used with CreateFile to connect to the device.
        /// </param>
        /// <param name="DeviceInterfaceGuid">
        /// Supplies the GUID for the device interface supported by this device.
        /// </param>
        public GenericWinUsbIoInterface(
            string DeviceInstanceId,
            string FriendlyName,
            string DeviceInterfaceSymbolicLinkName,
            Guid DeviceInterfaceGuid
            )
        {
            InterfaceType = InterfaceType.USB;
            this.DeviceInstanceId = DeviceInstanceId;
            this.FriendlyName = FriendlyName;
            this.DeviceInterfaceSymbolicLinkName = DeviceInterfaceSymbolicLinkName;
            this.DeviceInterfaceGuid = DeviceInterfaceGuid;
            UniqueIdentifier = DeviceInterfaceSymbolicLinkName;
        }

        #endregion

        #region Properties and Indexers

        /// <summary>
        /// This field uniquely identifies the IO interface and is essentially 
        /// the symbolic name for the device interface.
        /// </summary>
        public string UniqueIdentifier { get; }

        /// <summary>
        /// This field defines the device instance ID for the USB device. The
        /// device instance ID is also referred to as the device instance path.
        /// This value shows up as device Instance Path in the device Manager.
        /// </summary>
        public string DeviceInstanceId { get; set; }

        /// <summary>
        /// This field defines the friendly name reported by the USB device. 
        /// This value shows up as Friendly Name in the device Manager.
        /// </summary>
        public string FriendlyName { get; }

        /// <summary>
        /// This field defines the device interface path. This value can be used
        /// with CreateFile to connect to the device.
        /// </summary>
        public string DeviceInterfaceSymbolicLinkName { get; }

        /// <summary>
        /// This field defines the GUID for the device interface supported by 
        /// this device.
        /// </summary>
        public Guid DeviceInterfaceGuid { get; }

        /// <summary>
        /// This field defines the serial number for the device. This serial
        /// number is obtained from the Serial Number field in the standard
        /// USB device descriptor. If this is an empty string, the serial number
        /// might not have been specified or the device has not been opened for
        /// IO.
        /// </summary>
        public string SerialNumber { get; private set; }

        /// <summary>
        /// This field defines the state of the device.
        /// </summary>
        public bool IsOpened
        {
            get { return WinUsbIo != null; }
        }

        /// <summary>
        /// This field defines the type of the ioInterface.
        /// </summary>
        public InterfaceType InterfaceType { get; }

        /// <summary>
        /// This field contains an object of the WinUsbIo class for the USB 
        /// device. It is used to perform read, write and control tranfers to 
        /// the WinUsb device.
        /// </summary>
        internal WinUsbIo WinUsbIo { get; set; }

        /// <summary>
        /// This field contains mapping of the services available on this device
        /// and the local IP port number associated with that service via IP 
        /// over USB. This field will be null if the protocol is not supported 
        /// by this device.
        /// </summary>
        public Dictionary<string, uint> IpOverUsbServiceInfo { get; set; }

        /// <summary>
        /// Guid reported by IPOverUsb
        /// </summary>
        public Guid DeviceGuid { get; set; }

        #endregion

        #region Public Methods

        /// <summary>
        /// This routine is used to send WinUsb control transfers.
        /// </summary>
        /// <param name="setupPacket">
        /// Supplies the WinUsb setup packet for the control transfer.
        /// </param>
        /// <param name="buffer">
        /// Supplies the buffer that contains the data that needs to be sent
        /// along with the control transfer.
        /// </param>
        /// <param name="offset">
        /// Supplies the offset in bytes into the <paramref name="buffer"/> from
        /// which the data is to be transferred to the device.
        /// </param>
        /// <param name="count">
        /// Supplies the number of bytes of data to be transferred to the device
        /// from the <paramref name="buffer"/>.
        /// </param>
        /// <returns>
        /// Returns true if the control transfer succeeded and false otherwise.
        /// </returns>
        public bool SendControlTransferData(
            WinUsbSetupPacket setupPacket,
            byte[] buffer,
            int offset,
            int count
            )
        {
            if (IsOpened == false)
            {
                return false;
            }

            return WinUsbIo.SendControlTransferData(setupPacket,
                                                    buffer,
                                                    offset,
                                                    count);
        }

        /// <summary>
        /// This routine opens the device for read/write access.
        /// </summary>
        /// <returns>
        /// Returns true if the device was successfully opened for read/write
        /// access.
        /// </returns>
        public bool Open()
        {
            var status = true;
            if (IsOpened == false)
            {
                try
                {
                    WinUsbIo = new WinUsbIo(DeviceInterfaceSymbolicLinkName);
                }
                catch (Exception exception)
                {
                    if (exception.Message.Contains("is invalid"))
                    {
                        return false;
                    }

                    throw;
                }

                UsbStandardDeviceDescriptor descriptor;
                status = WinUsbIo.GetDeviceDescriptor(out descriptor);
                if (status)
                {
                    SerialNumber = descriptor.SerialNumber;
                }
            }

            return IsOpened && status;
        }

        /// <summary>
        /// This routine closes the device for read/write access.
        /// </summary>
        public void Close()
        {
            if (WinUsbIo != null)
            {
                WinUsbIo.Dispose();
                WinUsbIo = null;
            }
        }

        /// <summary>
        /// This routine writes data to the USB device.
        /// </summary>
        /// <param name="data">
        /// Supplies a buffer that contains the data to be written to the 
        /// device.
        /// </param>
        /// <param name="length">
        /// Supplies the number of bytes of <paramref name="data"/> to be 
        /// written to the USB device.
        /// </param>
        /// <returns>
        /// Returns true if the data could be written successfully to the device
        /// and false under the following circumstances:
        /// <list type="bullet">
        /// <item>
        /// The <paramref name="length"/> parameter specifying the number of 
        /// bytes in the <paramref name="data"/> buffer is either negative or
        /// more than the length of the buffer.
        /// </item>
        /// <item>
        /// The device has not been opened for read/write accesses.
        /// </item>
        /// <item>
        /// The device is inaccessible for writing.
        /// </item>
        /// </list>
        /// </returns>
        public bool Write(
            byte[] data,
            int length
            )
        {
            if (data == null)
            {
                return false;
            }

            if ((length > data.Length) || (length < 0))
            {
                return false;
            }

            if (IsOpened == false)
            {
                return false;
            }

            WinUsbIo.WriteWithTimeout(data, 0, length, 1000);
            return true;
        }

        /// <summary>
        /// This routine reads data from the USB device.
        /// </summary>
        /// <param name="buffer">
        /// Supplies a buffer that is used by the routine to return data read
        /// from the device.
        /// </param>
        /// <param name="length">
        /// Supplies the number of bytes in the <paramref name="buffer"/>.
        /// </param>
        /// <returns>
        /// Returns the number of bytes that were read from the device.
        /// </returns>
        public int Read(
            byte[] buffer,
            int length
            )
        {
            if (IsOpened == false)
            {
                return 0;
            }

            int rxSize = WinUsbIo.ReadWithTimeout(buffer,
                                                  0,
                                                  buffer.Length,
                                                  500);

            return rxSize;
        }

        /// <summary>
        /// This routine disposes off a GenericWinUsbIoInterface object.
        /// </summary>
        //
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        #endregion

        #region Private Methods

        private void Dispose(bool Disposing)
        {
            if (Disposing)
            {
                if (WinUsbIo != null)
                {
                    WinUsbIo.Dispose();
                    WinUsbIo = null;
                }
            }
        }

        #endregion
    }
}