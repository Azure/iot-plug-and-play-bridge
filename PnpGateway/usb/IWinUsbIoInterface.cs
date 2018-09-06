using System;
using System.Collections.Generic;
using static PnpGateway.NativeMethods;

namespace PnpGateway
{
    /// <summary>
    /// This interface defines the properties and methods for a WinUsb device.
    /// </summary>
    public interface IWinUsbIoInterface : IDeviceIoInterface
    {
        /// <summary>
        /// This field defines the device instance ID for the USB device. The
        /// device instance ID is also referred to as the device instance path.
        /// This value shows up as device Instance Path in the device Manager.
        /// </summary>
        string DeviceInstanceId { get; set; }

        /// <summary>
        /// This field defines the friendly name reported by the USB device. 
        /// This value shows up as Friendly Name in the device Manager.
        /// </summary>
        string FriendlyName { get; }

        /// <summary>
        /// This field defines the device interface path. This value can be used
        /// with CreateFile to connect to the device.
        /// </summary>
        string DeviceInterfaceSymbolicLinkName { get; }

        /// <summary>
        /// This field defines the GUID for the device interface supported by 
        /// this device.
        /// </summary>
        Guid DeviceInterfaceGuid { get; }

        /// <summary>
        /// This field defines the serial number for the device. This serial
        /// number is obtained from the Serial Number field in the standard
        /// USB device descriptor. If this is an empty string, the serial number
        /// might not have been specified or the device has not been opened for
        /// IO.
        /// </summary>
        string SerialNumber { get; }

        /// <summary>
        /// This field contains mapping of the services available on this device
        /// and the local IP port number associated with that service via IP 
        /// over USB. This field will be null if the protocol is not supported 
        /// by this device.
        /// </summary>
        Dictionary<string, uint> IpOverUsbServiceInfo { get; set; }

        /// <summary>
        /// Guid reported by IpOverUsb that is needed by TShell for multidut
        /// </summary>
        Guid DeviceGuid { get; }

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
        bool SendControlTransferData(
            WinUsbSetupPacket setupPacket,
            byte[] buffer,
            int offset,
            int count
            );
    }
}