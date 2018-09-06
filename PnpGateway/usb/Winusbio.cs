using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.IO.Ports;

namespace PnpGateway
{

        /// <summary>
        /// This structure defines the information that is used to construct a setup
        /// packet for a WinUsb device.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack = 1)]
        public struct WinUsbSetupPacket
        {
            /// <summary>
            /// This field defines the request type. The values that are assigned to
            /// this member are defined in Table 9.2 of section 9.3 of the Universal
            /// Serial Bus (USB) specification (www.usb.org).
            /// </summary>
            public byte RequestType;

            /// <summary>
            /// This field defines the device request. The values that are assigned 
            /// to this member are defined in Table 9.3 of section 9.4 of the 
            /// Universal Serial Bus (USB) specification.
            /// </summary>
            public byte Request;

            /// <summary>
            /// This field defines the value to be passed as a parameter for the
            /// request. The meaning of this member varies according to the request.
            /// For an explanation of this member, see the Universal Serial Bus 
            /// (USB) specification.
            /// </summary>
            public ushort Value;

            /// <summary>
            /// This field defines the index to be passed as a parameter for the
            /// request. The meaning of this member varies according to the request.
            /// For an explanation of this member, see the Universal Serial Bus 
            /// (USB) specification.
            /// </summary>
            public ushort Index;

            /// <summary>
            /// This field defines the number of bytes to transfer as part of the 
            /// USB request.
            /// </summary>
            public ushort Length;
        }

        internal class UsbStandardDeviceDescriptor
        {
            public static readonly byte Length = 18;
            public static readonly UsbDescriptorType DescriptorType = UsbDescriptorType.Device;
            public ushort UsbSpecificationReleaseNumber;
            public byte DeviceClass;
            public byte DeviceSubClass;
            public byte DeviceProtocol;
            public byte MaxPacketSize0;
            public ushort VendorId;
            public ushort ProductId;
            public ushort DeviceReleaseNumber;
            public string Manufacturer;
            public string Product;
            public string SerialNumber;
            public ushort NumConfigurations;
        }

        /// <summary>
        /// This class serves as a wrapper to interact with a WinUsb device.
        /// </summary>
        internal class WinUsbIo : IDisposable
        {
            #region Members

            private const byte UsbEndpointDirectionMask = 0x80;
            private string DeviceInterfaceSymbolicLinkName { get; }
            private readonly SafeFileHandle deviceHandle;
            private IntPtr usbHandle;
            private byte bulkInPipeId;
            private byte bulkOutPipeId;
            private bool isDisposed;

            #endregion

            #region Constructor

            /// <summary>
            /// This routine constructs the WinUsbIo object.
            /// </summary>
            /// <param name="deviceInterfaceSymbolicLinkName">
            /// Supplies the symbolic link name for the device interface that will 
            /// be used to interact with the WinUsb device.
            /// </param>
            public WinUsbIo(
                string deviceInterfaceSymbolicLinkName
                )
            {
                //
                // Validate the arguments.
                //

                if (string.IsNullOrEmpty(deviceInterfaceSymbolicLinkName))
                {
                    throw new ArgumentException("Invalid Argument", nameof(deviceInterfaceSymbolicLinkName));
                }

                isDisposed = false;

                //
                // Save the device name. We don't need to do this but this will be
                // helpful during debugging.
                //

                DeviceInterfaceSymbolicLinkName = deviceInterfaceSymbolicLinkName;

                try
                {
                    deviceHandle = CreateDeviceHandle(DeviceInterfaceSymbolicLinkName);

                    if (deviceHandle.IsInvalid)
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        throw new IOException(string.Format(CultureInfo.CurrentCulture, "Handle for {0} is invalid. Error: {1}", deviceInterfaceSymbolicLinkName, errorCode));
                    }

                    InitializeDevice();

                    if (false == ThreadPool.BindHandle(deviceHandle))
                    {
                        throw new IOException(string.Format(CultureInfo.CurrentCulture, "BindHandle on device {0} failed.", deviceInterfaceSymbolicLinkName));
                    }
                }
                catch (Exception)
                {
                    CloseDeviceHandle();
                    throw;
                }
            }

            #endregion

            #region Read/Write Methods

            /// <summary>
            /// This routine allows data to be read from the device.
            /// </summary>
            /// <param name="buffer">
            /// Supplies a buffer into which the data read from the device will be
            /// stored.
            /// </param>
            /// <param name="offset">
            /// Supplies an offset in bytes into the <paramref name="buffer"/> from
            /// which data will be stored.
            /// </param>
            /// <param name="count">
            /// Supplies the number of bytes to be read from the device into the 
            /// buffer.
            /// </param>
            /// <param name="timeout">
            /// Supplies the timeout in milliseconds after which the read should be
            /// aborted.
            /// </param>
            /// <returns>
            /// Returns the number of bytes read from the device.
            /// </returns>
            /// <exception cref="ObjectDisposedException">
            /// This exception is thrown if the device handle for the WinUsb device
            /// is invalid. This can happen if the device has been disconnected.
            /// </exception>
            /// <exception cref="ArgumentNullException">
            /// This exceptions is thrown when the <paramref name="buffer"/> 
            /// parameter is null.
            /// </exception>
            /// <exception cref="ArgumentOutOfRangeException">
            /// This exception is thrown when the <paramref name="offset"/>
            /// parameter or the <paramref name="count"/> parameters are negative
            /// numbers.
            /// </exception>
            /// <exception cref="ArgumentException">
            /// This exception is thrown if either the number of bytes specified by
            /// <paramref name="count"/> exceed the capacity of the 
            /// <paramref name="buffer"/> when the <paramref name="offset"/> is
            /// taken into account.
            /// </exception>
            /// <exception cref="Win32Exception">
            /// This exception is thrown when no data could be read from the device 
            /// because of some failure. This exception is not thrown if the timeout
            /// expired and no data was read.
            /// </exception>
            public unsafe int ReadWithTimeout(
                byte[] buffer,
                int offset,
                int count,
                uint timeout
                )
            {

                uint lengthTransferred;

                if (deviceHandle.IsClosed)
                {
                    throw new ObjectDisposedException("File closed");
                }

                if (buffer == null)
                {
                    throw new ArgumentNullException(nameof(buffer));
                }

                if (offset < 0)
                {
                    throw new ArgumentOutOfRangeException(nameof(offset), "ArgumentOutOfRange_NeedNonNegNum");
                }

                if (count < 0)
                {
                    throw new ArgumentOutOfRangeException(nameof(count), "ArgumentOutOfRange_NeedNonNegNum");
                }

                if ((buffer.Length - offset) < count)
                {
                    throw new ArgumentException(("Argument_InvalidOffLen"));
                }

                SetPipePolicy(bulkInPipeId,
                             (uint)WinUsbPolicyType.PipeTransferTimeout,
                             timeout);

                fixed (byte* bufferPtr = buffer)
                {
                    if (false == NativeMethods.WinUsbReadPipe(
                        usbHandle,
                        bulkInPipeId,
                        bufferPtr + offset,
                        (uint)count,
                        out lengthTransferred,
                        (NativeOverlapped*)IntPtr.Zero))
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        if ((int)WinError.SemaphoreTimeout != errorCode)
                        {
                            throw new Win32Exception(errorCode);
                        }
                    }
                }

                return (int)lengthTransferred;
            }

            /// <summary>
            /// This routine allows data to be written to the device.
            /// </summary>
            /// <param name="buffer">
            /// Supplies a buffer that contains the data that is to be written to 
            /// the device.
            /// </param>
            /// <param name="offset">
            /// Supplies an offset into the <paramref name="buffer"/> starting from
            /// which the data is to be written to the device.
            /// </param>
            /// <param name="count">
            /// Supplies the number of bytes to be written to the device.
            /// </param>
            /// <param name="timeout">
            /// Supplies the timeout in milliseconds after which the write should be
            /// aborted.
            /// </param>
            /// <returns>
            /// Returns true if the write succeeded and false otherwise.
            /// </returns>
            /// <exception cref="ObjectDisposedException">
            /// This exception is thrown if the device handle for the WinUsb device
            /// is invalid. This can happen if the device has been disconnected.
            /// </exception>
            /// <exception cref="ArgumentNullException">
            /// This exceptions is thrown when the <paramref name="buffer"/> 
            /// parameter is null.
            /// </exception>
            /// <exception cref="ArgumentOutOfRangeException">
            /// This exception is thrown when the <paramref name="offset"/>
            /// parameter or the <paramref name="count"/> parameters are negative
            /// numbers.
            /// </exception>
            /// <exception cref="ArgumentException">
            /// This exception is thrown if either the number of bytes specified by
            /// <paramref name="count"/> exceed the capacity of the 
            /// <paramref name="buffer"/> when the <paramref name="offset"/> is
            /// taken into account.
            /// </exception>
            /// <exception cref="Win32Exception">
            /// This exception is thrown when no data could be written to the device 
            /// because of some failure. This exception is not thrown if the timeout
            /// expired and no data was written.
            /// </exception>
            public unsafe bool WriteWithTimeout(
                byte[] buffer,
                int offset,
                int count,
                uint timeout
                )
            {
                if (deviceHandle.IsClosed)
                {
                    throw new ObjectDisposedException("File closed");
                }

                if (buffer == null)
                {
                    throw new ArgumentNullException(nameof(buffer));
                }

                if (offset < 0)
                {
                    throw new ArgumentOutOfRangeException(nameof(offset), "ArgumentOutOfRange_NeedNonNegNum");
                }

                if (count < 0)
                {
                    throw new ArgumentOutOfRangeException(nameof(count), "ArgumentOutOfRange_NeedNonNegNum");
                }

                if ((buffer.Length - offset) < count)
                {
                    throw new ArgumentException(("Argument_InvalidOffLen"));
                }

                SetPipePolicy(bulkOutPipeId,
                             (uint)WinUsbPolicyType.PipeTransferTimeout,
                             timeout);

                fixed (byte* bufferPtr = buffer)
                {
                    uint lengthTransferred;
                    if (false == NativeMethods.WinUsbWritePipe(
                        usbHandle,
                        bulkOutPipeId,
                        bufferPtr + offset,
                        (uint)count,
                        out lengthTransferred,
                        (NativeOverlapped*)IntPtr.Zero))
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        if ((int)WinError.SemaphoreTimeout != errorCode)
                        {
                            throw new Win32Exception(errorCode);
                        }
                    }
                }

                return true;
            }

            #endregion

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
            public unsafe bool SendControlTransferData(
                WinUsbSetupPacket setupPacket,
                byte[] buffer,
                int offset,
                int count
                )
            {
                byte* bufferPtr;
                GCHandle pinnedBuffer = GCHandle.Alloc(buffer, GCHandleType.Pinned);
                if (buffer == null)
                {
                    bufferPtr = (byte*)IntPtr.Zero;
                }
                else
                {
                    bufferPtr = (byte*)pinnedBuffer.AddrOfPinnedObject();
                }

                uint lengthTransferred;
                var status = NativeMethods.WinUsbControlTransfer(
                    usbHandle,
                    setupPacket,
                    bufferPtr + offset,
                    (uint)count,
                    out lengthTransferred,
                    (NativeOverlapped*)IntPtr.Zero);

                pinnedBuffer.Free();
                return status;
            }

            public bool GetDeviceDescriptor(
                out UsbStandardDeviceDescriptor descriptor
                )
            {

                descriptor = null;
                var buffer = new byte[UsbStandardDeviceDescriptor.Length];
                var status = GetDescriptor(UsbStandardDeviceDescriptor.DescriptorType,
                                           0,
                                           0,
                                           buffer);
                if (status == false)
                {
                    return false;
                }

                descriptor = new UsbStandardDeviceDescriptor
                {
                    UsbSpecificationReleaseNumber = BitConverter.ToUInt16(buffer, 2),
                    DeviceClass = buffer[4],
                    DeviceSubClass = buffer[5],
                    DeviceProtocol = buffer[6],
                    MaxPacketSize0 = buffer[7],
                    VendorId = BitConverter.ToUInt16(buffer, 8),
                    ProductId = BitConverter.ToUInt16(buffer, 10),
                    DeviceReleaseNumber = BitConverter.ToUInt16(buffer, 12),
                    NumConfigurations = buffer[17]
                };

                status = GetStringDescriptor(buffer[14],
                                             UsbLanguageId.EnglishUnitedStates,
                                             out descriptor.Manufacturer);

                if (status == false)
                {
                    return false;
                }

                status = GetStringDescriptor(buffer[15],
                                             UsbLanguageId.EnglishUnitedStates,
                                             out descriptor.Product);

                if (status == false)
                {
                    return false;
                }

                status = GetStringDescriptor(buffer[16],
                                             UsbLanguageId.EnglishUnitedStates,
                                             out descriptor.SerialNumber);

                if (status == false)
                {
                    return false;
                }

                return true;
            }

            public bool GetStringDescriptor(
                byte index,
                UsbLanguageId languageId,
                out string returnString
                )
            {

                returnString = null;
                var buffer = new byte[256];
                var status = GetDescriptor(UsbDescriptorType.String, index, languageId, buffer);
                if (status == false)
                {
                    return false;
                }

                returnString = Encoding.Unicode.GetString(buffer, 2, buffer[0] - 2);
                return true;
            }

            public unsafe bool GetDescriptor(
                UsbDescriptorType type,
                byte index,
                UsbLanguageId languageId,
                byte[] buffer
                )
            {
                fixed (byte* bufferPtr = buffer)
                {
                    uint lengthTransferred;
                    if (false == NativeMethods.WinUsbGetDescriptor(
                        usbHandle,
                        (byte)type,
                        index,
                        (ushort)languageId,
                        bufferPtr,
                        (uint)buffer.Length,
                        out lengthTransferred))
                    {
                        int errorCode = Marshal.GetLastWin32Error();
                        if ((int)WinError.InsufficientBuffer != errorCode)
                        {
                            throw new Win32Exception(errorCode);
                        }

                        return false;
                    }
                }

                return true;
            }

            #region Helper Methods

            /// <summary>
            /// Create the device Handle.
            /// </summary>
            /// <param name="deviceName">The device name.</param>
            /// <returns>
            /// The device File Handle.
            /// </returns>
            private static SafeFileHandle CreateDeviceHandle(
                string deviceName)
            {
                return NativeMethods.CreateFile(
                    deviceName,
                    (uint)(AccessRights.Read | AccessRights.Write),
                    (uint)(ShareModes.Read | ShareModes.Write),
                    IntPtr.Zero,
                    (uint)(CreateFileDisposition.CreateExisting),
                    (uint)FileAttributes.Normal | (uint)FileFlags.Overlapped,
                    IntPtr.Zero);
            }

            private void CloseDeviceHandle()
            {
                if (IntPtr.Zero != usbHandle)
                {
                    NativeMethods.WinUsbFree(usbHandle);
                }

                if (false == deviceHandle.IsInvalid &&
                    false == deviceHandle.IsClosed)
                {
                    deviceHandle.Close();
                }
            }

            private void InitializeDevice()
            {
                var interfaceDescriptor = new WinUsbInterfaceDescriptor();
                var pipeInfo = new WinUsbPipeInformation();

                if (false == NativeMethods.WinUsbInitialize(
                    deviceHandle,
                    ref usbHandle))
                {
                    throw new IOException("WinUsb Initialization failed.");
                }

                if (false == NativeMethods.WinUsbQueryInterfaceSettings(
                    usbHandle,
                    0,
                    ref interfaceDescriptor))
                {
                    throw new IOException("WinUsb Query Interface Settings failed.");
                }

                for (byte i = 0; i < interfaceDescriptor.NumEndpoints; i++)
                {
                    if (false == NativeMethods.WinUsbQueryPipe(
                        usbHandle,
                        0,
                        i,
                        ref pipeInfo))
                    {
                        throw new IOException("WinUsb Query Pipe Information failed");
                    }

                    switch (pipeInfo.PipeType)
                    {
                        case WinUsbPipeType.Bulk:
                            if (IsBulkInEndpoint(pipeInfo.PipeId))
                            {
                                SetupBulkInEndpoint(pipeInfo.PipeId);
                            }
                            else if (IsBulkOutEndpoint(pipeInfo.PipeId))
                            {
                                SetupBulkOutEndpoint(pipeInfo.PipeId);
                            }
                            else
                            {
                                throw new IOException("Invalid Endpoint Type.");
                            }
                            break;
                    }
                }
            }

            bool IsBulkInEndpoint(byte pipeId)
            {
                return (pipeId & UsbEndpointDirectionMask) == UsbEndpointDirectionMask;
            }

            bool IsBulkOutEndpoint(byte pipeId)
            {
                return (pipeId & UsbEndpointDirectionMask) == 0;
            }

            void SetupBulkInEndpoint(byte pipeId)
            {
                bulkInPipeId = pipeId;
            }

            void SetupBulkOutEndpoint(byte pipeId)
            {
                bulkOutPipeId = pipeId;
            }

            void SetPipePolicy(byte pipeId, uint policyType, uint value)
            {
                if (false == NativeMethods.WinUsbSetPipePolicy(
                    usbHandle,
                    pipeId,
                    policyType,
                    (uint)Marshal.SizeOf(typeof(uint)),
                    ref value))
                {
                    throw new IOException("WinUsb SetPipe Policy failed.");
                }
            }

            #endregion

            #region Dispose

            /// <summary>
            /// This routine disposes the object and closes the device handle.
            /// </summary>
            /// <param name="disposing">
            /// Supplies whether this routine was called in the context of a Dispose
            /// method or in the context of a finalizer.
            /// </param>
            protected void Dispose(bool disposing)
            {
                if (isDisposed) return;

                try
                {
                    CloseDeviceHandle();
                }
                catch (Exception)
                {
                    // ignored
                }
                finally
                {
                    isDisposed = true;
                }
            }

            /// <summary>
            /// This routine disposes the object and closes the device handle.
            /// </summary>
            public void Dispose()
            {
                Dispose(true);
                GC.SuppressFinalize(this);
            }

            /// <summary>
            /// This routine finalizes the object and closes the device handle.
            /// </summary>
            ~WinUsbIo()
            {
                // the handle may be already invalid, no need to disconnect anyway
                Dispose(false);
            }

            #endregion
        }
    }