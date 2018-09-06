using System;

namespace PnpGateway
{ 
    /// <summary>
    /// This enumeration defines the device IO interface type
    /// </summary>
    public enum InterfaceType
    {
        /// <summary>
        /// Unknown device
        /// </summary>
        Unknown,

        /// <summary>
        /// USB device
        /// </summary>
        USB,

        /// <summary>
        /// Serial Port device
        /// </summary>
        SerialPort,

        /// <summary>
        /// IP device
        /// </summary>
        IP
    };

    /// <summary>
    /// This interface defines the properties and methods for a generic device
    /// IO interface.
    /// </summary>
    public interface IDeviceIoInterface : IDisposable
    {

        /// <summary>
        /// This field defines the type of the device IO interface.
        /// </summary>
        InterfaceType InterfaceType { get; }

        /// <summary>
        /// This field defines a string that uniquely identifies this device IO
        /// interface.
        /// </summary>
        string UniqueIdentifier { get; }

        /// <summary>
        /// This field defines the state of the device IO interface.
        /// </summary>
        bool IsOpened { get; }

        /// <summary>
        /// This routine opens the device for read/write access.
        /// </summary>
        /// <returns>
        /// Returns true if the device was successfully opened for read/write
        /// access.
        /// </returns>
        bool Open();

        /// <summary>
        /// This routine closes the device for read/write access.
        /// </summary>
        void Close();

        /// <summary>
        /// This routine writes data to the device.
        /// </summary>
        /// <param name="data">
        /// Supplies a buffer that contains the data to be written to the 
        /// device.
        /// </param>
        /// <param name="length">
        /// Supplies the number of bytes of <paramref name="data"/> to be 
        /// written to the device.
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
        bool Write(
            byte[] data,
            int length
            );

        /// <summary>
        /// This routine reads data from the device.
        /// </summary>
        /// <param name="buffer">
        /// Supplies a buffer that is used by the routine to return data read
        /// from the device.
        /// </param>
        /// <param name="length">
        /// Supplies the number of bytes in the <paramref name="buffer"/>.
        /// </param>
        /// <returns>
        /// Returns the number of bytes read from the device.
        /// </returns>
        int Read(
            byte[] buffer,
            int length
            );
    }
}