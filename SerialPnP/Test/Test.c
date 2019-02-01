#include <stdio.h>
#include "SerialPnP.c"

// This function will be called during SerialPnP initialization, and should
// configure the serial port for SerialPnP operation, enabling it with a baud
// rate of 115200 and other settings compliant with the Serial PnP specification.
void
SerialPnP_PlatformSerialInit()
{

}

// This function should return the number of buffered characters available from
// the serial port used for SerialPnP operation.
unsigned int
SerialPnP_PlatformSerialAvailable()
{
    return 0;
}

// This function should return the next buffered character read from the
// serial port used for SerialPnP operation. If no characters are available,
// it shall return -1.
int
SerialPnP_PlatformSerialRead()
{
    return -1;
}

// This function should write a single character to the serial port to be used
// for SerialPnP operation.
void
SerialPnP_PlatformSerialWrite(
    char            Character
)
{
    if (Character == 0x5A) {
        printf("\n");
    }

    printf("0x%x ", (uint8_t) Character);
}

// This function should reset the state of the device.
void
SerialPnP_PlatformReset()
{

}

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
    *outputSuccess = 1;
}


void DumpDescriptor() {
    char* rawDescriptor = g_SerialPnPDescriptor->Content;
    uint16_t dOffset = 0;
    printf("Device version: %d name: %.*s\n", rawDescriptor[0], rawDescriptor[1], &rawDescriptor[2]);

    for (SerialPnPDescriptorEntry* entry = g_SerialPnPDescriptor->Next;
         entry != 0;
         entry = entry->Next)
    {
        rawDescriptor = entry->Content;

        printf("Descriptor entry type: ");

        dOffset = 1;

        switch (rawDescriptor[0]) {
        case SERIALPNP_DESCRIPTORTYPE_INTERFACE:
            {
                uint16_t uriLength = (rawDescriptor[1] | (rawDescriptor[2] << 8));
                printf(" INTERFACE\n");
                printf("  ID: %.*s\n", uriLength, rawDescriptor + 3);
            }
            break;

        case SERIALPNP_DESCRIPTORTYPE_EVENT:
            {
                printf(" EVENT\n");
                printf(" NAME     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DISP NAME: %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DESC     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" UNITS    : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;
            }
            break;

        case SERIALPNP_DESCRIPTORTYPE_COMMAND:
            {
                printf(" COMMAND\n");
                printf(" NAME     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DISP NAME: %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DESC     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;
            }
            break;

        case SERIALPNP_DESCRIPTORTYPE_PROPERTY:
            {
                printf(" PROPERTY\n");
                printf(" NAME     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DISP NAME: %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" DESC     : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;

                printf(" UNITS    : %.*s\n", rawDescriptor[dOffset], rawDescriptor + dOffset + 1);
                dOffset += rawDescriptor[dOffset] + 1;
            }
            break;

        default:
            printf("UNKNOWN\n");
        }
    }
}

void
ASSERT(bool condition, const char* str) {
    if (!condition) {
        printf("FAIL: %s\n", str);
    } else {
        printf("PASS: %s\n", str);
    }
}

int main() {
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

    // Traverse and print descriptor
    DumpDescriptor();

    ASSERT(SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_PROPERTY,
                                  "sample_rate",
                                  11) != 0,
           "Verify found property callback correctly");

    ASSERT(SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_PROPERTY,
                                  "s4mple_rate",
                                  11) == 0,
           "Verify didn't find invalid property callback");

    ASSERT(SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_PROPERTY,
                                  "sample_rates",
                                  12) == 0,
           "Verify didn't find invalid property callback");

    ASSERT(SerialPnP_FindCallback(SERIALPNP_DESCRIPTORTYPE_COMMAND,
                                  "sample_rate",
                                  11) == 0,
           "Verify didn't find invalid method callback");

    printf("Testing send event functionality...");
    SerialPnP_SendEventFloat("temperature", 10.12f);
    printf("\n");
}