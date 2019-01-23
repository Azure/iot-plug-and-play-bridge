using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace PnpGateway.Serial
{
    public class RxPacketContainer
    {
        public byte[] Packet = null;
        public byte PacketType = 0;
        public EventWaitHandle Event = new EventWaitHandle(false, EventResetMode.ManualReset);
    }

    class SerialPnPPacketInterface
    {
        public event EventHandler<byte[]> PacketReceieved = null;

        private byte[] RxBuffer = new byte[4096]; // Todo: maximum buffer size
        private uint RxBufferIndex = 0;
        private bool RxEscaped = false;

        private static Mutex RxPacketQueueMutex = new Mutex();
        private List<RxPacketContainer> RxPacketQueue = new List<RxPacketContainer>();

        private SerialPort Port;

        public SerialPnPPacketInterface(String ComPort)
        {
            // Physical characteristics as defined in spec
            Port = new SerialPort(ComPort, 115200, Parity.None, 8, StopBits.One);
            Port.ReceivedBytesThreshold = 1;
            Port.DataReceived += RxCallback;
        }

        public void Open()
        {
            if (Port == null)
            {
                throw new Exception("Port does not exist");
            }

            Port.Open();
        }

        public void Close()
        {
            if (Port == null)
            {
                throw new Exception("Port does not exist");
            }

            Port.Close();
        }

        public void TxPacket(byte[] OutPacket)
        {
            int txLength = 1 + OutPacket.Length;
            // First iterate through and find out our new length
            foreach(byte x in OutPacket)
            {
                if ((x == 0x5A) || (x == 0xEF))
                {
                    txLength++;
                }
            }

            // Now construct outgoing buffer
            byte[] TxPacket = new byte[txLength];
            txLength = 1;
            TxPacket[0] = 0x5A; // Start of frame
            foreach (byte x in OutPacket)
            {
                // Escape these bytes where necessary
                if ((x == 0x5A) || (x == 0xEF))
                {
                    TxPacket[txLength++] = 0xEF;
                    TxPacket[txLength++] = (byte) (x - 1);
                } else
                {
                    TxPacket[txLength++] = x;
                }
            }

            //Console.Write("TX: ");
            //foreach (byte x in TxPacket)
            //{
            //    Console.Write(x.ToString() + " ");
            //}
           // Console.Write("\n");

            // Transmit packet on serial bus
            Port.Write(TxPacket, 0, TxPacket.Length);
        }

        private void RxCallback(object sender, SerialDataReceivedEventArgs e)
        {
            int inb;

            while (Port.BytesToRead > 0)
            {
                inb = Port.ReadByte();

                //if (inb == 0x5A)
                //{
                    //Console.Out.Write("\n");
                //}
                //Console.Out.Write(inb.ToString() + " ");

                // Check for a start of packet byte
                if (inb == 0x5A)
                {
                    RxBufferIndex = 0;
                    RxEscaped = false;
                    continue;
                }

                // Check for an escape byte
                if (inb == 0xEF)
                {
                    RxEscaped = true;
                    continue;
                }

                // If last byte was an escape byte, increment current byte by 1
                if (RxEscaped)
                {
                    inb++;
                    RxEscaped = false;
                }

                RxBuffer[RxBufferIndex++] = (byte)inb;

                if (RxBufferIndex >= 4096)
                {
                    throw new Exception("Filled Rx buffer. Protocol is bad.");
                }

                // Minimum packet length is 4, so once we are >= 4 begin checking
                // the receieve buffer length against the length field.
                if (RxBufferIndex >= 4)
                {
                    ushort PacketLength = (ushort)((RxBuffer[0]) | (RxBuffer[1] << 8)); // LSB first, L-endian

                    if (RxBufferIndex == PacketLength)
                    {
                        byte[] NewPacket = new byte[RxBufferIndex];
                        Buffer.BlockCopy(RxBuffer, 0, NewPacket, 0, (int)RxBufferIndex);
                        RxBufferIndex = 0; // This should be reset anyway

                        // Deliver the newly receieved packet
                        //Console.Out.Write("\n");
                        this.DeliverRxPacket(NewPacket);
                    }
                }
            }
        }
    
        public async Task<byte[]> RxPacket(byte PacketType)
        {
            // Synchronously put ourselves into the RxPacketQueue
            var recipient = new RxPacketContainer();
            recipient.PacketType = PacketType;

            RxPacketQueueMutex.WaitOne();
            RxPacketQueue.Add(recipient);
            RxPacketQueueMutex.ReleaseMutex();

            await Task.Run(() =>
            {
                recipient.Event.WaitOne();
            });

            return recipient.Packet;
        }

        private void DeliverRxPacket(byte[] packet)
        {
            RxPacketContainer recipient = null;

            // If there's something in the wait queue, deliver packet to it rather
            // than invoking generic callback.
            RxPacketQueueMutex.WaitOne();
            for (int i = 0; i < RxPacketQueue.Count; i++)
            {
                var waiter = RxPacketQueue[i];

                if (waiter.PacketType == packet[2])
                {
                    recipient = RxPacketQueue[i];
                    RxPacketQueue.RemoveAt(i);
                }
            }
            RxPacketQueueMutex.ReleaseMutex();

            // Deliver it to waiting recipient
            if (recipient != null)
            {
                recipient.Packet = packet;
                recipient.Event.Set();
            }
            else
            {
                // Call generic new packet handler
                this.PacketReceieved?.Invoke(this, packet);
            }
        }
    }
}
