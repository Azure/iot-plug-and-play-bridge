# SerialPnP
Device-side module allowing for self-identification of Azure PnP interfaces
over a serial connection.

### Description
This library implements Serial PnP protocol communication functionality, and provides
a simple interface which can be used by device builders on any microcontroller platform
supporting a C compiler, to easily expose interfaces and data from an MCU powered device
to a gateway or aggregator over a serial connection.

### Features
The library implements the following features
- Communication via Serial PnP protocol
- Construction of device descriptor
- Reporting of event telemetry from device
- Dispatches calls to property and method handlers

### In development
- Support for full range of data schema. At present, only `float` and `int32_t` are supported.
- Does not gracefully handle memory allocation failures.
- Only supports a single interface per device.

### Getting Started (All Platforms)

#### Adding SerialPnP library to your project
To get started using the library, bring the `SerialPnP.c` and `SerialPnP.h` files into your
project directory and add them to your project. The library requires platform-specific functionality
to be implemented by the developer or platform implementer. The platform must also provide `malloc` and `free` functions.

These functions are defined in `SerialPnP.h`:
```
// This function will be called during SerialPnP initialization, and should
// configure the serial port for SerialPnP operation, enabling it with a baud
// rate of 115200 and other settings compliant with the Serial PnP specification.
void
SerialPnP_PlatformSerialInit();

// This function should return the number of buffered characters available from
// the serial port used for SerialPnP operation.
unsigned int
SerialPnP_PlatformSerialAvailable();

// This function should return the next buffered character read from the
// serial port used for SerialPnP operation. If no characters are available,
// it shall return -1.
int
SerialPnP_PlatformSerialRead();

// This function should write a single character to the serial port to be used
// for SerialPnP operation.
void
SerialPnP_PlatformSerialWrite(
    char            Character
);

// This function should reset the state of the device.
void
SerialPnP_PlatformReset();
```

The above functions are expected to provide support for serial and reset functionality.
For serial, the implementation of these functions should maintain a suitable buffer which
is asynchronously accessed by SerialPnP using the above functions.

Example implementation of these functions is available in [ArduinoSerialPnP.cpp](./ArduinoExample/ArduinoSerialPnP.cpp), which demonstrates
how the Arduino standard library functions are wrapped to provide SerialPnP support.

#### SerialPnP initialization
To expose interfaces using SerialPnP, the library must first be initialized.
These steps should be taken during your device startup - the device will not be
accessible over the serial port until initialization is complete.

First, call `SerialPnP_Setup("Device Name")`, providing the name of your device in place of `Device Name`. This call
should only be made once in your firmware.

#### Interface definition
Your device may implement one or more PnP interfaces. These interfaces support a
subset of Azure PnP interface functionality. An interface is made up of one or more
of the following types of features:
- `Event` - provides for a one way notification of telemetry data from the device to the gateway.
  May be used, for example, for a periodic temperature measurement or other sample.
- `Command` - provides a method call interface allowing the gateway to call functions on the device with
  an input parameter, and allows the device to return an output value to the gateway.
- `Property` - provides a mutable or immutable property on the device which may be read and written from
  the gateway.

An interface is defined by calling `SerialPnP_NewInterface`, followed by `New` calls for
each of the features provided by said interface.

#### Example initialization & definition
The following example shows an excerpt from the initialization method of an example
thermometer device:
```
// This call must be made exactly once.
SerialPnP_Setup("Example Thermometer");

// This begins configuration of a new Azure PnP interface, backed by the
// provided URI.
SerialPnP_NewInterface("http://contoso.com/thermometer_example");

// This provides an event, which will be periodically sent by the device to
// provide a temperature measurement.
SerialPnP_NewEvent("temperature",                          // short ID for the event
                   "Ambient Temperature",                  // friendly name for event
                   "A sample of the ambient temperature.", // description of event
                   SerialPnPSchema_Float,                  // data schema for temperature data
                   "celsius");                             // units of the temperature data

// This provides a property which the gateway may use to configure the sample rate
// for the temperature data.
SerialPnP_NewProperty("sample_rate",                             // short ID for property
                      "Sample Rate",                             // friendly name for property
                      "Sample Rate of temperature measurements", // description of property
                      "ms",                                      // unit of sample rate is millisecond
                      SerialPnPSchema_Int,                       // data schema of property
                      false,                                     // required
                      true,                                      // writeable by gateway
                      (SerialPnPCb*) CbSampleRate);              // callback - see below

// This provides a calibration method which allows the gateway to provide an ambient
// temperature for calibration purposes.
SerialPnP_NewCommand("calibrate",                                // short ID for command
                     "Calibrate Temperature",                    // friendly name for command
                     "Calibrates the thermometer",               // description of command
                     SerialPnPSchema_Float,                      // data schema of input to command
                     SerialPnPSchema_Boolean,                    // data schema of output to command
                     (SerialPnPCb*) CbCalibrate);                // callback - see below

// This call must be made once to complete Serial PnP Setup, after all interaces have been defined.
// It will notify the host that device initialization is complete.
SerialPnP_Ready();
```

#### SerialPnP Callbacks
The SerialPnP library handles serialization and execution of gateway driven functionality
to achieve the following:
- Reading properties from device
- Writing properties to device
- Executing commands on device

The library defines a `SerialPnPCb` method signature as below:
```typedef void (*SerialPnPCb)(void*, void*);```

Callback methods do not return a value, but rather operate on the two pointer
parameters. The first parameter will be input, and the second will be output.

No typing is provided for the parameters - a callback for a given property and command
is expected to know what schema is selected as part of input definition and have its
method declared appropriately.

Property callbacks can be called in two ways:
1. The input parameter is null, and only the output parameter is defined.
    - In this case, the callback is being used to read a property. The callback should place the current value of the property into the variable pointed to by the output parameter. Operating on the input parameter when it is null will result in instability.
2. Both the input parameter and output parameters are defined.
    - In this case the callback is being used to write a property. The callback should perform any necessary validation on the input parameter, update the property if validation succeeds, and place the new value of the property into the output value.

Command callbacks will be called with both input and output parameters defined. Command callbacks should read the input parameter and write the output parameter as relevant to the functionality of the callback in question.

An example implementation of the property and command callbacks as used in our example
thermometer can be seen below:
```
// This global variable dictates the sample rate.
uint32_t g_SampleRate = 1000; // default = 1000ms

// This function will be called by SerialPnP to both read and update the sample
// rate of the temperature. The schema of the property data is `SerialPnPSchema_Int`
// as defined on initialization, so the method signature is defined accordingly.
void
CbSampleRate(
    int32_t     *input,
    int32_t     *output
)
{
    // If input variable is defined, validate and update sample rate variable
    if (input) {
        // Only allow sample rates between 100ms and 10s
        if ((*input > 100) && (*input <= 10000)) {
            g_SampleRate = *input;
        }
    }

    // Output will always be defined for a property callback
    *output = g_SampleRate;
}

// This function handles calibration and will return success or failure as a bool.
void
CbCalibrate(
    float       *inputTemperature,
    bool        *outputSuccess
)
{
    *outputSuccess = DoActualCalibrationHere(*inputTemperature);
}
```

#### Handling communication
Once the interfaces have been defined and callbacks are implemented, device firmware
must be updated to regularly call `SerialPnP_Process()`, to process any outstanding
SerialPnP operations. `SerialPnP_SendEvent..` should also be called to send new
telemetry from the device to gateway. The following `SerialPnP_SendEvent` functions are
provided:
- `SerialPnP_SendEventInt(const char* EventShortId, int32_t Value)`
- `SerialPnP_SendEventFloat(const char* EventShortId, float Value)`

#### Examples
Please see [ArduinoSerialPnP.cpp](./ArduinoExample/ArduinoSerialPnP.cpp) for an example implementation of the SerialPnP library on an Arduino and [ArduinoExample.ino](./ArduinoExample/ArduinoExample.ino) for example usage of the SerialPnP library on an Arduino device.
