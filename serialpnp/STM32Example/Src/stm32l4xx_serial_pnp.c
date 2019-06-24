#include "stdio.h"
#include <stdlib.h>
#include "string.h"
#include "SerialPnP.h"
#include "stm32l4xx_hal.h"

#define RX_BUFFER_MAX 512       // Max-limited buffer size of HAL_UART_Receive
#define RECEIVED_DATA_STRGING_SIZE 1024 // Max storage size of received data

char uart_rx_buffer[RECEIVED_DATA_STRGING_SIZE]; // Storage received data from HAL_UART_Receive
int start = 0;  // Index of started reading of uart_rx_buffer
int end = 0;    // Index of started writing of uart_rx_buffer
UART_HandleTypeDef* uart;

void SerialPnP_InitUART(UART_HandleTypeDef* huart){
  uart = huart;
}

// This function will be called during SerialPnP initialization, and should
// configure the serial port for SerialPnP operation, enabling it with a baud
// rate of 115200 and other settings compliant with the Serial PnP specification.
void
SerialPnP_PlatformSerialInit()
{
  //uart->Instance = UART4;
  uart->Init.BaudRate = 115200;
  uart->Init.WordLength = UART_WORDLENGTH_8B;
  uart->Init.StopBits = UART_STOPBITS_1;
  uart->Init.Parity = UART_PARITY_NONE;
  uart->Init.Mode = UART_MODE_TX_RX;
  uart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  uart->Init.OverSampling = UART_OVERSAMPLING_16;
  uart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  uart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(uart) != HAL_OK)
  {
    Error_Handler();
  }
}

// This function should return the number of buffered characters available from
// the serial port used for SerialPnP operation.
unsigned int
SerialPnP_PlatformSerialAvailable()
{
    char temp_buf[RX_BUFFER_MAX];
    int dataSize = 0;
    HAL_UART_Receive_v2(uart, (uint8_t*)temp_buf, RX_BUFFER_MAX, 30, &dataSize); // 30ms is the least timeout of HAL_UART_Receive
    if(dataSize > 0){
      for(int i = 0;i < dataSize;i++){
        uart_rx_buffer[end % RECEIVED_DATA_STRGING_SIZE] = temp_buf[i];
        end++;
      }
    }
    return end - start;
}


// This function should return the next buffered character read from the
// serial port used for SerialPnP operation. If no characters are available,
// it shall return -1.
int
SerialPnP_PlatformSerialRead()
{
  char ch = uart_rx_buffer[start % RECEIVED_DATA_STRGING_SIZE];
  start++;
  return ch;
}

// This function should write a single character to the serial port to be used
// for SerialPnP operation.
void
SerialPnP_PlatformSerialWrite(
    char            Character
)
{  
   if (HAL_OK != HAL_UART_Transmit(uart, (uint8_t *) &Character, 1, 1000))
  {
      Error_Handler();
  }
}

// This function should reset the state of the device.
void
SerialPnP_PlatformReset()
{
    // This call must be made once to complete Serial PnP Setup, after all interaces have been defined.
    // It will notify the host that device initialization is complete.
    SerialPnP_Ready();
}
