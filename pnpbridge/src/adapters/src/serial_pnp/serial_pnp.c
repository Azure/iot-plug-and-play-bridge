// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include <pnpadapter_api.h>

#include "azure_c_shared_utility/azure_base32.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/singlylinkedlist.h"
#include "azure_c_shared_utility/lock.h"

// TODO: Fix this missing reference
#ifndef AZURE_UNREFERENCED_PARAMETER
#define AZURE_UNREFERENCED_PARAMETER(param)   (void)(param)
#endif

#ifdef WIN32
#include <Windows.h>
#include <cfgmgr32.h>
#include <ctype.h>
#include <winerror.h>
#else
typedef unsigned int DWORD;
typedef uint8_t byte;
typedef int HANDLE;
typedef short USHORT;
typedef uint16_t UINT16;

#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <termios.h>
#include <unistd.h>
#endif

#include "parson.h"

#include "serial_pnp.h"

int SerialPnp_UartReceiver(
    void* context)
{
    int result = 0;
    PSERIAL_DEVICE_CONTEXT deviceContext = (PSERIAL_DEVICE_CONTEXT)context;

    while (result >= 0) {
        byte* desc = NULL;
        DWORD length;

        SerialPnp_RxPacket(deviceContext, &desc, &length, 0x00);
        if (desc != NULL)
        {
            SerialPnp_UnsolicitedPacket(deviceContext, desc, length);
            free(desc);
        }
    }

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT SerialPnp_TxPacket(
    PSERIAL_DEVICE_CONTEXT serialDevice,
    byte* OutPacket,
    int Length)
{
    DWORD write_size = 0;
    int error = 0;

    DWORD txLength = 1 + Length; // "+1" for start of frame byte
    // First iterate through and find out our new length
    for (int i = 0; i < Length; i++)
    {
        if ((SERIALPNP_START_OF_FRAME_BYTE == OutPacket[i]) || (SERIALPNP_ESCAPE_BYTE == OutPacket[i]))
        {
            txLength++;
        }
    }

    // Now construct outgoing buffer
    byte* SerialPnp_TxPacket = malloc(sizeof(byte) * txLength);
    if (!SerialPnp_TxPacket)
    {
        LogError("Error out of memory");
        return IOTHUB_CLIENT_ERROR;
    }
    txLength = 1;
    SerialPnp_TxPacket[0] = 0x5A; // Start of frame
    for (int i = 0; i < Length; i++)
    {
        // Escape these bytes where necessary
        if ((SERIALPNP_START_OF_FRAME_BYTE == OutPacket[i]) || (SERIALPNP_ESCAPE_BYTE == OutPacket[i]))
        {
            SerialPnp_TxPacket[txLength++] = SERIALPNP_ESCAPE_BYTE;
            SerialPnp_TxPacket[txLength++] = (byte)(OutPacket[i] - 1);
        }
        else
        {
            SerialPnp_TxPacket[txLength++] = OutPacket[i];
        }
    }


#ifdef WIN32
    if (!WriteFile(serialDevice->hSerial, SerialPnp_TxPacket, txLength, &write_size, &serialDevice->osWriter))
    {
        // Write returned immediately, but is asynchronous
        if (ERROR_IO_PENDING != (error = GetLastError()))
        {
            // Write returned actual error and not just pending
            LogError("write failed: %d", error);
            return IOTHUB_CLIENT_ERROR;
        }
        else
        {
            if (!GetOverlappedResult(serialDevice->hSerial, &serialDevice->osWriter, &write_size, TRUE))
            {
                error = GetLastError();
                LogError("write failed: %d", error);
                return IOTHUB_CLIENT_ERROR;
            }
        }
    }
#else
    if ((write_size = write(serialDevice->hSerial, (const void *)SerialPnp_TxPacket, (size_t)txLength)) == -1)
    {
        LogError("write failed");
        error = -1;
    }
#endif

    if (write_size != txLength)
    {
        LogError("Timeout while writing");
        return IOTHUB_CLIENT_INDEFINITE_TIME;
    }
    free(SerialPnp_TxPacket);
    return IOTHUB_CLIENT_OK;
}

const EventDefinition* SerialPnp_LookupEvent(
    SINGLYLINKEDLIST_HANDLE interfaceDefinitions,
    char* EventName,
    int InterfaceId)
{
    const InterfaceDefinition* interfaceDef;
    LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);

    for (int i = 0; i < InterfaceId - 1; i++)
    {
        if (NULL == interfaceDefHandle)
        {
            return NULL;
        }
        interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
    }
    interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

    SINGLYLINKEDLIST_HANDLE events = interfaceDef->Events;
    LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(events);
    const EventDefinition* ev;
    while (NULL != eventDef)
    {
        ev = singlylinkedlist_item_get_value(eventDef);
        if (strcmp(ev->defintion.Name, EventName) == 0)
        {
            return ev;
        }
        eventDef = singlylinkedlist_get_next_item(eventDef);
    }

    return NULL;
}

byte* SerialPnp_StringSchemaToBinary(
    Schema schema,
    byte* buffer,
    int* length)
{
    byte* bd = NULL;

    // Following allows for parameterless commands
    if (schema == Void)
    {
        return bd;
    }
    char* data = (char*)buffer;

    if ((Float == schema) || (Int == schema))
    {
        bd = malloc(sizeof(byte) * 4);
        if (!bd)
        {
            LogError("Error out of memory");
            return NULL;
        }
        *length = 4;

        if (schema == Float)
        {
            float x = 0;
            x = (float)atof(data);
            memcpy(bd, &x, sizeof(float));
        }
        else if (schema == Int)
        {
            int x;
            x = atoi(data);
            memcpy(bd, &x, sizeof(int));
        }
    }
    else if (Boolean == schema)
    {
        bd = malloc(sizeof(byte) * 1);
        if (!bd)
        {
            LogError("Error out of memory");
            return NULL;
        }

        *length = 1;
        if (strcmp(data, "true"))
        {
            bd[0] = 1;
        }
        else if (strcmp(data, "false"))
        {
            bd[0] = 0;
        }
        else
        {
            free(bd);
            *length = 0;
            bd = NULL;
        }
    }
    else
    {
        LogError("Unknown schema");
    }

    return bd;
}

char* SerialPnp_BinarySchemaToString(
    Schema schema,
    byte* Data,
    byte length)
{
    const int MAXRXSTRLEN = 12; // longest case INT_MIN = -2147483648 (11 characters + 1)
    char* rxstrdata = malloc(MAXRXSTRLEN);
    if (!rxstrdata)
    {
        LogError("Error out of memory");
        return NULL;
    }

    if ((Float == schema) && (4 == length))
    {
        sprintf_s(rxstrdata, MAXRXSTRLEN, "%.6f", *((float*)Data));
    }
    else if (((Int == schema) && (4 == length)))
    {
        sprintf_s(rxstrdata, MAXRXSTRLEN, "%d", *((int*)Data));
    }
    // These next two are values that should be returned from toggle_properties on Arduino 2Do ???
    else if ((Int == schema) && (*(int*)Data == SERIAL_PNP_RESPONSE_ON))
    {
        // When set by toggle
        sprintf_s(rxstrdata, MAXRXSTRLEN, "%d", *((int*)Data));
    }
    else if ((Int == schema) && (*(int*)Data == SERIAL_PNP_RESPONSE_OFF))
    {
        // When clear by toggle
        sprintf_s(rxstrdata, MAXRXSTRLEN, "%d", *((int*)Data));
    }
    else if (((Boolean == schema) && (1 == length)))
    {
        sprintf_s(rxstrdata, MAXRXSTRLEN, "%d", *((int*)Data));
    }
    else if (((Void == schema) ))
    {
        sprintf_s(rxstrdata, MAXRXSTRLEN, "");
    }
    else
    {
        LogError("Unknown schema");
        free(rxstrdata);
        return NULL;
    }

    return rxstrdata;
}

const PropertyDefinition* SerialPnp_LookupProperty(
    SINGLYLINKEDLIST_HANDLE interfaceDefinitions,
    const char* propertyName,
    int InterfaceId)
{
    LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
    const InterfaceDefinition* interfaceDef;

    for (int i = 0; i < InterfaceId - 1; i++)
    {
        if (NULL == interfaceDefHandle)
        {
            return NULL;
        }
        interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
    }
    interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);
    if (NULL == interfaceDef)
    {
        return NULL;
    }

    SINGLYLINKEDLIST_HANDLE property = interfaceDef->Properties;
    LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(property);
    const PropertyDefinition* ev;
    while (NULL != eventDef)
    {
        ev = singlylinkedlist_item_get_value(eventDef);
        if (strcmp(ev->defintion.Name, propertyName) == 0)
        {
            return ev;
        }
        eventDef = singlylinkedlist_get_next_item(eventDef);
    }

    return NULL;
}


JSON_Value* Properties = NULL;

void SerialPnp_UnsolicitedPacket(
    PSERIAL_DEVICE_CONTEXT device,
    byte* packet,
    DWORD length)
{
;
    // Got an event
    if (SERIALPNP_PACKET_TYPE_EVENT_NOTIFICATION == packet[SERIALPNP_PACKET_PACKET_TYPE_OFFSET])
    {
        byte rxInterfaceId = packet[SERIALPNP_PACKET_INTERFACE_NUMBER_OFFSET];
        byte rxNameLength = packet[SERIALPNP_PACKET_NAME_LENGTH_OFFSET];
        DWORD rxDataSize = length - rxNameLength - SERIALPNP_PACKET_NAME_OFFSET;

        char* event_name = malloc(sizeof(char) * (rxNameLength + 1));
        if (!event_name)
        {
            LogError("Error out of memory");
            return;
        }
        memcpy(event_name, packet + SERIALPNP_PACKET_NAME_OFFSET, rxNameLength);
        event_name[rxNameLength] = '\0';

#ifdef WIN32
#ifdef ARDUINO_SERIAL
        /************************************************************************

                In serial_pnp.h
                #define ARDUINO_SERIAL
                2Do: Would be nice to infer it from from config.json

                This enables top level (in ArduinoExample.ino) calls in the Arduino Serial example such as :
                 SerialPnP_SendEventString("DEBUG", "hello");

                 requires in the Arduino sample, SerialPnP.c :

                void
                SerialPnP_SendEventString(
                    const char*     Name,
                    const char*     Value
                )
                {
                    SerialPnP_SendEventRaw(Name, (void*) Value, strlen(Value)+1);
                }

                and in header SerialPnP.h in the Arduino sample:

                void
                SerialPnP_SendEventString(
                    const char*     Name,
                    const char*     Value
                );

                This captures the packet dispays the value and disposes of it.
                Use the Dummy event "DEBUG"
                You don't do a create event for that.

        ************************************************************************/
        if (strncmp("DEBUG", event_name, strlen("DEBUG")) == 0)
        {
            char* rxDataStrn = malloc(1 + sizeof(byte) * rxDataSize);
            memcpy(rxDataStrn, packet + SERIALPNP_PACKET_NAME_OFFSET + rxNameLength, rxDataSize);
            rxDataStrn[rxDataSize] = '\0';
            LogInfo("== DEBUG: %s",rxDataStrn);
            free(rxDataStrn);
            free(event_name);
            return;
        }
        else if (strncmp("CLEAR_PROPERTIES", event_name, strlen("CLEAR_PROPERTIES")) == 0)
        {
            if (Properties == NULL)
            {
                LogInfo("== CLEAR PROPERTIES: Already clear. Ignoring.");
            }
            else {
                json_value_free(Properties);
                Properties = NULL;
                LogInfo("== CLEAR_PROPERTIES: %s", "");
            }
            free(event_name);
            return;
        }
#endif //ARDUINO_SERIAL
#endif //WIN32

        const EventDefinition* ev = SerialPnp_LookupEvent(device->InterfaceDefinitions, event_name, rxInterfaceId);
        if (!ev)
        {
            const PropertyDefinition* pd = SerialPnp_LookupProperty(device->InterfaceDefinitions, event_name, rxInterfaceId);
            if (!pd)
            {
                LogError("Couldn't find event or property: %s", event_name);
                return;
            }
            else
            {
                //if(Properties == NULL)
                    //Properties = json_value_init_array();

                if (Properties == NULL)
                    Properties = json_value_init_array();
                //JSON_Object* jv;
                char* rxDataStrn = malloc(1 + sizeof(byte) * rxDataSize);
                memcpy(rxDataStrn, packet + SERIALPNP_PACKET_NAME_OFFSET + rxNameLength, rxDataSize);
                rxDataStrn[rxDataSize] = '\0';

                JSON_Array* leaves = json_value_get_array(Properties);
                JSON_Value* leaf_value = json_value_init_object();
                JSON_Object* leaf_object = json_value_get_object(leaf_value);
                json_object_set_string(leaf_object, event_name, rxDataStrn);
                json_array_append_value(leaves, leaf_value);

                //JSON_Array*leaves2 = json_value_get_array(Properties);
                //size_t num_procs = json_array_get_count(leaves);
                for (int i = 0; i < json_array_get_count(leaves); i++) {
                    JSON_Object * leaf2 = json_array_get_object(leaves, i);
                    const char * name = json_object_get_name(leaf2,0);
                    const char* val = json_object_get_string(leaf2,name);
                    LogInfo("SerialPnp Property: %s Value: %s",name,val);
                }

                free(rxDataStrn);
                return;
            }
        }

        byte* rxData = malloc(sizeof(byte) * rxDataSize);
        if (!rxData)
        {
            LogError("Error out of memory");
            free(event_name);
            return;
        }

        memcpy(rxData, packet + SERIALPNP_PACKET_NAME_OFFSET + rxNameLength, rxDataSize);

        char* rxstrdata = SerialPnp_BinarySchemaToString(ev->DataSchema, rxData, (byte)rxDataSize);
        if (!rxstrdata)
        {
            LogError("Unknown schema");
            free(event_name);
            free(rxData);
            return;
        }
        LogInfo("%s: %s", ev->defintion.Name, rxstrdata);

        //SerialPnp_SendEventAsync(device, ev->defintion.Name, rxstrdata);
        SerialPnp_SendEventwithPropertiesAsync(device, ev->defintion.Name, rxstrdata, Properties);
        free(event_name);
        free(rxData);
    }
    // Got a property update
    else if (SERIALPNP_PACKET_TYPE_PROPERTY_NOTIFICATION == packet[SERIALPNP_PACKET_PACKET_TYPE_OFFSET])
    {
        byte rxNameLength = packet[SERIALPNP_PACKET_NAME_LENGTH_OFFSET];
        char* prop = malloc(sizeof(char) * (rxNameLength + 1));
        memcpy(prop, packet + SERIALPNP_PACKET_NAME_OFFSET, rxNameLength);
        prop[rxNameLength] = '\0';
        LogInfo("Serial Pnp Adapter: Got Property Update Notification. propertyName= %s", prop);
        free(prop);
    }
}

const CommandDefinition* SerialPnp_LookupCommand(
    SINGLYLINKEDLIST_HANDLE interfaceDefinitions,
    const char* commandName,
    int InterfaceId)
{
    LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(interfaceDefinitions);
    const InterfaceDefinition* interfaceDef;

    for (int i = 0; i < InterfaceId - 1; i++)
    {
        if (NULL == interfaceDefHandle)
        {
            return NULL;
        }
        interfaceDefHandle = singlylinkedlist_get_next_item(interfaceDefHandle);
    }
    interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

    SINGLYLINKEDLIST_HANDLE command = interfaceDef->Commands;
    LIST_ITEM_HANDLE eventDef = singlylinkedlist_get_head_item(command);
    const CommandDefinition* ev;
    while (NULL != eventDef)
    {
        ev = singlylinkedlist_item_get_value(eventDef);
        if (strcmp(ev->defintion.Name, commandName) == 0)
        {
            return ev;
        }
        eventDef = singlylinkedlist_get_next_item(eventDef);
    }

    return NULL;
}

IOTHUB_CLIENT_RESULT SerialPnp_PropertyHandler(
    PSERIAL_DEVICE_CONTEXT serialDevice,
    const char* property,
    char* data)
{
    const PropertyDefinition* prop = SerialPnp_LookupProperty(serialDevice->InterfaceDefinitions, property, 0);
    byte* input = (byte*)data;

    if (NULL == prop)
    {
        LogInfo("NOTE: Ignoring Property Update from Device Twin for property %s as not in the Schema.", property);
        return IOTHUB_CLIENT_ERROR;
    }

    // Otherwise serialize data
    int dataLength = 0;
    byte* inputPayload = SerialPnp_StringSchemaToBinary(prop->DataSchema, input, &dataLength);
    if (!inputPayload)
    {
        return IOTHUB_CLIENT_ERROR;
    }

    int nameLength = (int)strlen(property);
    int txlength = SERIALPNP_PACKET_NAME_OFFSET + nameLength + dataLength;
    byte* txPacket = malloc(txlength);
    if (!txPacket)
    {
        LogError("Error out of memory");
        free(inputPayload);
        return IOTHUB_CLIENT_ERROR;
    }

    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET] = (byte)(txlength & 0xFF);
    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET + 1] = (byte)(txlength >> 8);
    txPacket[SERIALPNP_PACKET_PACKET_TYPE_OFFSET] = SERIALPNP_PACKET_TYPE_PROPERTY_REQUEST;
    // txPacket[3] is reserved
    txPacket[SERIALPNP_PACKET_INTERFACE_NUMBER_OFFSET] = (byte)0;
    txPacket[SERIALPNP_PACKET_NAME_LENGTH_OFFSET] = (byte)nameLength;

    memcpy(txPacket + SERIALPNP_PACKET_NAME_OFFSET, property, nameLength);
    memcpy(txPacket + SERIALPNP_PACKET_NAME_OFFSET + nameLength, inputPayload, dataLength);

    LogInfo("Setting property %s to %s", property, input);

    SerialPnp_TxPacket(serialDevice, txPacket, txlength);

    free(inputPayload);
    free(txPacket);

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT SerialPnp_CommandHandler(
    PSERIAL_DEVICE_CONTEXT serialDevice,
    const char* command,
    char* data,
    char** response)
{
    Lock(serialDevice->CommandLock);

    const CommandDefinition* cmd = SerialPnp_LookupCommand(serialDevice->InterfaceDefinitions, command, 0);
  
    byte* input = (byte*)data;

    if (NULL == cmd)
    {
        Unlock(serialDevice->CommandLock);
        return IOTHUB_CLIENT_ERROR;
    }

    // Otherwise serialize data
    int length = 0;
    Schema schema = cmd->RequestSchema;
    ///////////////////////////////////////////
    // Following enables parameterless commands
    if ( cmd->RequestSchema  == Invalid)
       schema = Void;
    ///////////////////////////////////////////
    byte* inputPayload = SerialPnp_StringSchemaToBinary(schema, input, &length);
    if ((!inputPayload)&& (schema != Void))
    {
        Unlock(serialDevice->CommandLock);
        return IOTHUB_CLIENT_ERROR;
    }

    int nameLength = (int)strlen(command);
    int txlength = SERIALPNP_PACKET_NAME_OFFSET + nameLength + length;
    byte* txPacket = malloc(txlength);
    if (!txPacket)
    {
        LogError("Error out of memory");
        free(inputPayload);
        Unlock(serialDevice->CommandLock);
        return IOTHUB_CLIENT_ERROR;
    }

    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET] = (byte)(txlength & 0xFF);
    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET + 1] = (byte)(txlength >> 8);
    txPacket[SERIALPNP_PACKET_PACKET_TYPE_OFFSET] = SERIALPNP_PACKET_TYPE_COMMAND_REQUEST;
    // txPacket[3] is reserved
    txPacket[SERIALPNP_PACKET_INTERFACE_NUMBER_OFFSET] = (byte)0;
    txPacket[SERIALPNP_PACKET_NAME_LENGTH_OFFSET] = (byte)nameLength;

    memcpy(txPacket + SERIALPNP_PACKET_NAME_OFFSET, command, nameLength);
    memcpy(txPacket + SERIALPNP_PACKET_NAME_OFFSET + nameLength, inputPayload, length);

    LogInfo("Invoking command %s to %s", command, input);

    if (IOTHUB_CLIENT_OK != SerialPnp_TxPacket(serialDevice, txPacket, txlength))
    {
        LogError("Error: command not sent to device.");
        free(inputPayload);
        free(txPacket);
        Unlock(serialDevice->CommandLock);
        return IOTHUB_CLIENT_ERROR;
    }

    Lock(serialDevice->CommandResponseWaitLock);
    if (COND_OK != Condition_Wait(serialDevice->CommandResponseWaitCondition, serialDevice->CommandResponseWaitLock, 60000))
    {
        LogError("Timeout waiting for response from device");
        free(inputPayload);
        free(txPacket);
        Unlock(serialDevice->CommandLock);
        Unlock(serialDevice->CommandResponseWaitLock);
        return IOTHUB_CLIENT_ERROR;
    }
    Unlock(serialDevice->CommandResponseWaitLock);

    byte* responsePacket = serialDevice->pbMainBuffer;
    int dataOffset = SERIALPNP_PACKET_NAME_OFFSET + nameLength;
    byte* commandResponse = responsePacket + dataOffset;
    char* stval = SerialPnp_BinarySchemaToString(cmd->ResponseSchema, commandResponse, (byte)length);
    free(inputPayload);
    free(responsePacket);
    free(txPacket);

    if (!stval)
    {
        Unlock(serialDevice->CommandLock);
        return IOTHUB_CLIENT_ERROR;
    }

    *response = stval;
    Unlock(serialDevice->CommandLock);
    return IOTHUB_CLIENT_OK;
}

void SerialPnp_ParseDescriptor(
    SINGLYLINKEDLIST_HANDLE interfaceDefinitions,
    byte* descriptor,
    DWORD length)
{
    int c = 4;

    byte version = descriptor[c++];
    byte display_name_length = descriptor[c++];

    char display_name[128];
    memcpy(display_name, descriptor + c, display_name_length);
    display_name[display_name_length] = '\0';

    LogInfo("Device Version : %d", version);
    LogInfo("Device Name    : %s", display_name);

    c += display_name_length;

    while (c < (int)length)
    {
        if (descriptor[c++] == 0x05)
        {
            InterfaceDefinition* indef = calloc(1, sizeof(InterfaceDefinition));
            indef->Events = singlylinkedlist_create();
            indef->Properties = singlylinkedlist_create();
            indef->Commands = singlylinkedlist_create();

            // parse ID5
            UINT16 interface_id_length = descriptor[c] | (descriptor[c + 1] << 8);
            c += 2;

            char* interface_id = malloc(sizeof(char)*(interface_id_length + 1));
            if (!interface_id)
            {
                LogError("Error out of memory");
                free(indef);
                return;
            }
            memcpy(interface_id, descriptor + c, interface_id_length);
            interface_id[interface_id_length] = '\0';
            LogInfo("Interface ID : %s", interface_id);
            c += interface_id_length;

            indef->Id = interface_id;

            // now process the different types
            while (c < (int)length)
            {
                if (descriptor[c] > 0x03) break; // not in a type of nested entry anymore

                FieldDefinition fielddef = { 0 };

                // extract common field properties
                byte ptype = descriptor[c++];
                byte pname_length = descriptor[c++];

                char* pname = malloc(sizeof(char)*(pname_length + 1));
                if (!pname)
                {
                    LogError("Error out of memory");
                    free(interface_id);
                    free(indef);
                    return;
                }
                memcpy(pname, descriptor + c, pname_length);
                pname[pname_length] = '\0';
                c += pname_length;

                byte pdisplay_name_length = descriptor[c++];
                char* pdisplay_name = malloc(sizeof(char)*(pdisplay_name_length + 1));
                if (!pdisplay_name)
                {
                    LogError("Error out of memory");
                    free(interface_id);
                    free(pname);
                    free(indef);
                    return;
                }
                memcpy(pdisplay_name, descriptor + c, pdisplay_name_length);
                pdisplay_name[pdisplay_name_length] = '\0';
                c += pdisplay_name_length;

                byte pdescription_length = descriptor[c++];
                char* pdescription = malloc(sizeof(char)*(pdescription_length + 1));
                if (!pdescription)
                {
                    LogError("Error out of memory");
                    free(interface_id);
                    free(pname);
                    free(pdisplay_name);
                    free(indef);
                    return;
                }
                memcpy(pdescription, descriptor + c, pdescription_length);
                pdescription[pdescription_length] = '\0';
                c += pdescription_length;

                LogInfo("\tProperty type : %d", ptype);
                LogInfo("\tName : %s", pname);
                LogInfo("\tDisplay Name : %s", pdisplay_name);
                LogInfo("\tDescription : %s", pdescription);

                fielddef.Name = pname;
                fielddef.DisplayName = pdisplay_name;
                fielddef.Description = pdescription;

                if (ptype == 0x01) // command
                {
                    CommandDefinition* tfdef = calloc(1, sizeof(CommandDefinition));
                    if (!tfdef)
                    {
                        LogError("Error out of memory");
                        free(interface_id);
                        free(pname);
                        free(pdisplay_name);
                        free(pdescription);
                        free(indef);
                        return;
                    }
                    tfdef->defintion = fielddef;

                    UINT16 prequest_schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
                    c += 2;
                    UINT16 presponse_schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
                    c += 2;

                    tfdef->RequestSchema = prequest_schema;
                    tfdef->ResponseSchema = presponse_schema;

                    singlylinkedlist_add(indef->Commands, tfdef);
                }
                else if (ptype == 0x02) // property
                {
                    PropertyDefinition* tfdef = calloc(1, sizeof(PropertyDefinition));
                    if (!tfdef)
                    {
                        LogError("Error out of memory");
                        free(interface_id);
                        free(pname);
                        free(pdisplay_name);
                        free(pdescription);
                        free(indef);
                        return;
                    }
                    tfdef->defintion = fielddef;

                    byte punit_length = descriptor[c++];
                    char* punit = malloc(sizeof(char) * (punit_length + 1));
                    if (!punit)
                    {
                        LogError("Unexpected error");
                        free(interface_id);
                        free(pname);
                        free(pdisplay_name);
                        free(pdescription);
                        free(tfdef);
                        free(indef);
                        return;
                    }
                    memcpy(punit, descriptor + c, punit_length);
                    punit[punit_length] = '\0';
                    c += punit_length;

                    LogInfo("\tUnit : %s", punit);

                    UINT16 schema = descriptor[c] | (descriptor[c + 1] << 8);
                    c += 2;
                    tfdef->DataSchema = schema;

                    byte flags = descriptor[c++];

                    bool prequired = (flags & (1 << 1)) != 0;
                    bool pwriteable = (flags & (1 << 0)) != 0;

                    tfdef->Units = punit;
                    tfdef->Required = prequired;
                    tfdef->Writeable = pwriteable;

                    singlylinkedlist_add(indef->Properties, tfdef);
                }
                else if (ptype == 0x03) // event
                {
                    EventDefinition* tfdef = calloc(1, sizeof(EventDefinition));
                    if (!tfdef)
                    {
                        LogError("Unexpected error");
                        free(interface_id);
                        free(pname);
                        free(pdisplay_name);
                        free(pdescription);
                        free(indef);
                        return;
                    }
                    tfdef->defintion = fielddef;

                    byte punit_length = descriptor[c++];
                    char* punit = malloc(sizeof(char) * (punit_length + 1));
                    if (!punit)
                    {
                        LogError("Error out of memory");
                        free(interface_id);
                        free(pname);
                        free(pdisplay_name);
                        free(pdescription);
                        free(tfdef);
                        free(indef);
                        return;
                    }
                    memcpy(punit, descriptor + c, punit_length);
                    punit[punit_length] = '\0';
                    c += punit_length;

                    LogInfo("\tUnit : %s", punit);

                    UINT16 schema = (UINT16)(descriptor[c] | (descriptor[c + 1] << 8));
                    c += 2;
                    tfdef->DataSchema = schema;
                    tfdef->Units = punit;

                    singlylinkedlist_add(indef->Events, tfdef);
                }
            }

            singlylinkedlist_add(interfaceDefinitions, indef);
        }
        else
        {
            LogError("Unsupported descriptor\n");
        }
    }
}

typedef struct _SERIAL_DEVICE
{
    char* InterfaceName;
} SERIAL_DEVICE, *PSERIAL_DEVICE;

SINGLYLINKEDLIST_HANDLE SerialDeviceList = NULL;
int SerialDeviceCount = 0;

#ifdef WIN32
IOTHUB_CLIENT_RESULT SerialPnp_FindSerialDevices()
{
    CONFIGRET cmResult = CR_SUCCESS;
    char* deviceInterfaceList;
    ULONG bufferSize = 0;

    SerialDeviceList = singlylinkedlist_create();

    cmResult = CM_Get_Device_Interface_List_Size(
        &bufferSize,
        (LPGUID)&GUID_DEVINTERFACE_COMPORT,
        NULL,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (CR_SUCCESS != cmResult)
    {
        return IOTHUB_CLIENT_ERROR;
    }

    deviceInterfaceList = malloc(bufferSize * sizeof(char));
    if (!deviceInterfaceList)
    {
        LogError("Error out of memory");
        return IOTHUB_CLIENT_ERROR;
    }

    cmResult = CM_Get_Device_Interface_ListA(
        (LPGUID)&GUID_DEVINTERFACE_COMPORT,
        NULL,
        deviceInterfaceList,
        bufferSize,
        CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    if (CR_SUCCESS != cmResult)
    {
        return IOTHUB_CLIENT_ERROR;
    }

    for (PCHAR currentDeviceInterface = deviceInterfaceList;
        *currentDeviceInterface != '\0';
        currentDeviceInterface += strlen(currentDeviceInterface) + 1)
    {
        PSERIAL_DEVICE serialDevs = malloc(sizeof(SERIAL_DEVICE));
        if (!serialDevs)
        {
            LogError("Error out of memory");
            free(deviceInterfaceList);
            return IOTHUB_CLIENT_ERROR;
        }
        serialDevs->InterfaceName = malloc((strlen(currentDeviceInterface) + 1) * sizeof(char));
        if (!serialDevs->InterfaceName)
        {
            LogError("Error out of memory");
            free(deviceInterfaceList);
            free(serialDevs);
            return IOTHUB_CLIENT_ERROR;
        }
        strcpy_s(serialDevs->InterfaceName, strlen(currentDeviceInterface) + 1, currentDeviceInterface);
        singlylinkedlist_add(SerialDeviceList, serialDevs);
        SerialDeviceCount++;
    }

    return IOTHUB_CLIENT_OK;
}
#endif

int SerialPnp_ParseInterfaceConfig(
    void* context)
{
    byte* desc;
    DWORD length;
    PSERIAL_DEVICE_CONTEXT deviceContext = context;
    int retries = SERIALPNP_RESET_OR_DESCRIPTOR_MAX_RETRIES;
    while (IOTHUB_CLIENT_OK != SerialPnp_ResetDevice(deviceContext))
    {
        LogError("Error sending reset request. Retrying...");
        if (0 == --retries)
        {
            LogError("Error exceeded max number of reset request retries. ");
            return IOTHUB_CLIENT_ERROR;
        }
        ThreadAPI_Sleep(5000);
    }
    retries = SERIALPNP_RESET_OR_DESCRIPTOR_MAX_RETRIES;
    while (IOTHUB_CLIENT_OK != SerialPnp_DeviceDescriptorRequest(deviceContext, &desc, &length))
    {
        LogError("Descriptor response not received. Retrying...");
        if (0 == --retries)
        {
            LogError("Error exceeded max number of descriptor request retries. ");
            return IOTHUB_CLIENT_ERROR;
        }
        ThreadAPI_Sleep(5000);
    }

    SerialPnp_ParseDescriptor(deviceContext->InterfaceDefinitions, desc, length);
    return IOTHUB_CLIENT_OK;
}

#ifndef WIN32
int set_interface_attribs(
    int fd,
    int speed,
    int parity)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        LogError("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
                                                    // disable IGNBRK for mismatched speed tests; otherwise receive break
                                                    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN] = 1;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    //tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    //tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        LogError("error %d from tcsetattr", errno);
        return -1;
    }
    return IOTHUB_CLIENT_OK;
}

void
set_blocking(
    int fd,
    int should_block)
{
    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0)
    {
        LogError("error %d from tggetattr", errno);
        return;
    }

    tty.c_cc[VMIN] = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
        LogError("error %d setting term attributes", errno);
}
#endif 

IOTHUB_CLIENT_RESULT SerialPnp_OpenDevice(
    const char* port,
    DWORD baudRate,
    PSERIAL_DEVICE_CONTEXT deviceContext)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
#ifdef WIN32
    HANDLE hSerial = CreateFileA(port,
        GENERIC_READ | GENERIC_WRITE,
        0, // must be opened with exclusive-access
        0, // NULL, no security attributes
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        0);

    if (hSerial == INVALID_HANDLE_VALUE)
    {
        // Handle the error
        int error = GetLastError();
        LogError("Failed to open com port %s, %x", port, error);
        if (error == ERROR_FILE_NOT_FOUND)
        {
            result = IOTHUB_CLIENT_INVALID_ARG;
            goto exit;
        }
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams))
    {
        return -1;
    }
    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams))
    {
        //error setting serial port state
        return -1;
    }

    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    if (!SetCommTimeouts(hSerial, &timeouts))
    {
        return -1;
    }
#else 
    char *portname = "/dev/ttyACM0";
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        LogError("error %d opening %s: %s", errno, portname, strerror(errno));
        return -1;
    }

    set_interface_attribs(fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)

#endif

#ifdef WIN32
    deviceContext->hSerial = hSerial;
    deviceContext->osWriter.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    deviceContext->osReader.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == deviceContext->osWriter.hEvent ||
        NULL == deviceContext->osReader.hEvent)
    {
        if (NULL != deviceContext->osWriter.hEvent)
        {
            CloseHandle(deviceContext->osWriter.hEvent);
        }
        if (NULL != deviceContext->osReader.hEvent)
        {
            CloseHandle(deviceContext->osReader.hEvent);
        }
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }
#else 
    deviceContext->hSerial = fd;
#endif

exit:
    return result;
}

IOTHUB_CLIENT_RESULT SerialPnp_RxPacket(
    PSERIAL_DEVICE_CONTEXT serialDevice,
    byte** receivedPacket,
    DWORD* length,
    char packetType)
{
    byte inb = 0;
    DWORD dwRead = 0;
    *receivedPacket = NULL;
    *length = 0;
    int error = 0;

#ifdef WIN32
    while (true)
    {
        if (!ReadFile(serialDevice->hSerial, &inb, 1, &dwRead, &serialDevice->osReader)) // if completed asynchronously, wait. 
        {
            if (ERROR_IO_PENDING != (error = GetLastError()))
            {
                // Read returned actual error and not just pending
                LogError("read failed: %d", error);
                return IOTHUB_CLIENT_ERROR;
            }
            else
            {
                if (!GetOverlappedResult(serialDevice->hSerial, &serialDevice->osReader, &dwRead, TRUE))
                {
                    error = GetLastError();
                    LogError("read failed: %d", error);
                    return IOTHUB_CLIENT_ERROR;
                }
            }

        }

        // Read can be successful but with no bytes actually read, shouldn't happen though
        if (dwRead == 0)
        {
            continue;
        }
        //LogInfo("read completed successfully");
#else
    while ((dwRead = read(serialDevice->hSerial, (void*)&inb, 1)) != -1)
    {
#endif
        // Check for a start of packet byte
        if (SERIALPNP_START_OF_FRAME_BYTE == inb)
        {
            serialDevice->RxBufferIndex = 0;
            serialDevice->RxEscaped = false;
            continue;
        }

        // Check for an escape byte
        if (SERIALPNP_ESCAPE_BYTE == inb)
        {
            serialDevice->RxEscaped = true;
            continue;
        }

        // If last byte was an escape byte, increment current byte by 1
        if (serialDevice->RxEscaped)
        {
            inb++;
            serialDevice->RxEscaped = false;
        }

        serialDevice->RxBuffer[serialDevice->RxBufferIndex++] = (byte)inb;

        if (serialDevice->RxBufferIndex >= MAX_BUFFER_SIZE)
        {
            LogError("Filled Rx buffer. Protocol is bad.");
            return IOTHUB_CLIENT_ERROR;
        }

        // Minimum packet length is 4, so once we are >= 4 begin checking
        // the receieve buffer length against the length field.
        if (serialDevice->RxBufferIndex >= SERIALPNP_MIN_PACKET_LENGTH)
        {
            int PacketLength = (int)((serialDevice->RxBuffer[0]) | (serialDevice->RxBuffer[1] << 8)); // LSB first, L-endian

            if (((int)serialDevice->RxBufferIndex == PacketLength) && (packetType == 0x00 || packetType == serialDevice->RxBuffer[2]))
            {
                *receivedPacket = malloc(serialDevice->RxBufferIndex * sizeof(byte));
                if (NULL == *receivedPacket)
                {
                    LogError("Error out of memory");
                    return IOTHUB_CLIENT_ERROR;
                }
                *length = serialDevice->RxBufferIndex;
                memcpy(*receivedPacket, serialDevice->RxBuffer, serialDevice->RxBufferIndex);
                serialDevice->RxBufferIndex = 0; // This should be reset anyway. Deliver the newly receieved packet

                // both the main thread and this thread can be waiting for packets, 
                // but this function that does the reading only runs on this thread
                // command responses are expected on the main thread
                if (SERIALPNP_PACKET_TYPE_COMMAND_RESPONSE == serialDevice->RxBuffer[SERIALPNP_PACKET_PACKET_TYPE_OFFSET])
                {
                    // signal the main thread in this case, pass back the buffer 
                    // using the pointer in the serial context instead of the return value
                    serialDevice->pbMainBuffer = *receivedPacket;
                    *receivedPacket = NULL;
                    Lock(serialDevice->CommandResponseWaitLock);
                    Condition_Post(serialDevice->CommandResponseWaitCondition);
                    Unlock(serialDevice->CommandResponseWaitLock);
                }
                break;
            }
        }
    }
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT SerialPnp_ResetDevice(
    PSERIAL_DEVICE_CONTEXT serialDevice)
{
    IOTHUB_CLIENT_RESULT error = IOTHUB_CLIENT_OK;
    // Prepare packet
    byte resetPacket[3] = { 0 }; // packet header
    byte* responsePacket = NULL;
    resetPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET] = 4; // length 4
    resetPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET + 1] = 0;
    resetPacket[SERIALPNP_PACKET_PACKET_TYPE_OFFSET] = SERIALPNP_PACKET_TYPE_RESET_REQUEST;

    // Send the new packet
    if (IOTHUB_CLIENT_OK != SerialPnp_TxPacket(serialDevice, resetPacket, 4))
    {
        LogError("Error sending request packet");
        error =  IOTHUB_CLIENT_ERROR;
        goto exit;
    }
    LogInfo("Sent reset request");

    DWORD length;
    if (IOTHUB_CLIENT_OK != SerialPnp_RxPacket(serialDevice, &responsePacket, &length, 0x02))
    {
        LogError("Error receiving response packet");
        error = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    if (NULL == responsePacket)
    {
        LogError("received NULL for response packet");
        error = IOTHUB_CLIENT_ERROR;
    }

    if (SERIALPNP_PACKET_TYPE_RESET_RESPONSE != responsePacket[2])
    {
        LogError("Bad reset response");
        error = IOTHUB_CLIENT_ERROR;
        goto exit;
    }

    if (IOTHUB_CLIENT_OK == error)
    {
        LogInfo("Receieved reset response");
    }

exit:
    if (responsePacket)
    {
        free(responsePacket);
    }
    return error;
}

IOTHUB_CLIENT_RESULT SerialPnp_DeviceDescriptorRequest(
    PSERIAL_DEVICE_CONTEXT serialDevice,
    byte** desc,
    DWORD* length)
{
    // Prepare packet
    byte txPacket[3] = { 0 }; // packet header
    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET] = 4; // length 4
    txPacket[SERIALPNP_PACKET_PACKET_LENGTH_OFFSET + 1] = 0;
    txPacket[SERIALPNP_PACKET_PACKET_TYPE_OFFSET] = SERIALPNP_PACKET_TYPE_DESCRIPTOR_REQUEST;

    // Send the new packets
    if (IOTHUB_CLIENT_OK != SerialPnp_TxPacket(serialDevice, txPacket, 4))
    {
        LogError("Error sending request packet");
        return IOTHUB_CLIENT_ERROR;
    }
    LogInfo("Sent descriptor request");

    if (IOTHUB_CLIENT_OK != SerialPnp_RxPacket(serialDevice, desc, length, 0x04))
    {
        LogError("Error receiving response packet");
        free(*desc);
        return IOTHUB_CLIENT_ERROR;
    }

    if (NULL == *desc)
    {
        LogError("received NULL for response packet");
        free(*desc);

        return IOTHUB_CLIENT_ERROR;
    }

    if (SERIALPNP_PACKET_TYPE_DESCRIPTOR_RESPONSE != (*desc)[2])
    {
        LogError("Bad descriptor response");
        free(*desc);
        return IOTHUB_CLIENT_ERROR;
    }

    LogInfo("Receieved descriptor response, of length %d", *length);
    return IOTHUB_CLIENT_OK;
}

void SerialPnp_SendEventCallback(
    IOTHUB_CLIENT_CONFIRMATION_RESULT pnpSendEventStatus,
    void* userContextCallback)
{
    LogInfo("SerialDataSendEventCallback called, result=%d, telemetry=%s", pnpSendEventStatus, (char*) userContextCallback);
}

IOTHUB_CLIENT_RESULT SerialPnp_SendEventwithPropertiesAsync(
    PSERIAL_DEVICE_CONTEXT DeviceContext,
    char* TelemetryName,
    char* TelemetryData,
    JSON_Value* _Properties)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    char telemetryMessageData[512] = { 0 };
    sprintf(telemetryMessageData, "{\"%s\":%s}", TelemetryName, TelemetryData);

    if ((messageHandle = PnP_CreateTelemetrywithPropertiesMessageHandle(DeviceContext->ComponentName, telemetryMessageData, _Properties)) == NULL)
    {
        LogError("Serial Pnp Adapter: PnP_CreateTelemetryMessageHandle failed.");
    }
    else if ((result = PnpBridgeClient_SendEventAsync(DeviceContext->ClientHandle, messageHandle,
        SerialPnp_SendEventCallback, (void*)TelemetryName)) != IOTHUB_CLIENT_OK)
    {
        LogError("Serial Pnp Adapter: IoTHub client call to _SendEventAsync failed, error=%d", result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return result;
}

IOTHUB_CLIENT_RESULT SerialPnp_SendEventAsync(
    PSERIAL_DEVICE_CONTEXT DeviceContext,
    char* TelemetryName,
    char* TelemetryData)
{
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;
    IOTHUB_MESSAGE_HANDLE messageHandle = NULL;

    char telemetryMessageData[512] = { 0 };
    sprintf(telemetryMessageData, "{\"%s\":%s}", TelemetryName, TelemetryData);

    if ((messageHandle = PnP_CreateTelemetryMessageHandle(DeviceContext->ComponentName, telemetryMessageData)) == NULL)
    {
        LogError("Serial Pnp Adapter: PnP_CreateTelemetryMessageHandle failed.");
    }
    else if ((result = PnpBridgeClient_SendEventAsync(DeviceContext->ClientHandle, messageHandle,
            SerialPnp_SendEventCallback, (void*)TelemetryName)) != IOTHUB_CLIENT_OK)
    {
        LogError("Serial Pnp Adapter: IoTHub client call to _SendEventAsync failed, error=%d", result);
    }

    IoTHubMessage_Destroy(messageHandle);

    return result;
}

int SerialPnp_GetListCount(
    SINGLYLINKEDLIST_HANDLE list)
{
    if (NULL == list)
    {
        return 0;
    }

    LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(list);
    int count = 0;
    while (NULL != item)
    {
        count++;
        item = singlylinkedlist_get_next_item(item);
    }
    return count;
}

static void SerialPnp_PropertyUpdateHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* PropertyName,
    JSON_Value* PropertyValue,
    int version,
    void* userContextCallback)
{
    AZURE_UNREFERENCED_PARAMETER(version);
    IOTHUB_CLIENT_RESULT iothubClientResult;
    PSERIAL_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);

    STRING_HANDLE jsonToSend = NULL;
    // const char * PropertyValueString = json_value_get_string(PropertyValue);
    char* PropertyValueString = NULL;
    const char* propertyValueString;
    if (json_value_get_type(PropertyValue) == JSONString)
    {
        propertyValueString = json_value_get_string(PropertyValue);
        if (propertyValueString != NULL)
        {
            PropertyValueString = malloc(strlen(propertyValueString + 1));
            strcpy(PropertyValueString, propertyValueString);
        }
        else
        {
            PropertyValueString = malloc(1);
            PropertyValueString[0] = (char)0;
        }
        
    }
    else if (json_value_get_type(PropertyValue) == JSONNumber)
    {
        // If a number, assumes it to be an int.  2Do allow for decimals.
        int iVal = (int) json_value_get_number(PropertyValue);
        PropertyValueString = malloc(20);
        sprintf(PropertyValueString, "%d", iVal);
    }
    else if (json_value_get_type(PropertyValue) == JSONBoolean)
    {
        bool iVal = json_value_get_boolean(PropertyValue);
        PropertyValueString = malloc(5);
        propertyValueString = (iVal == 1) ? "true" : "false";
        strcpy(PropertyValueString, propertyValueString);
    }
  
    size_t PropertyValueLen = strlen(PropertyValueString);

    int propertyCount = 0;

    if (NULL != deviceContext)
    {
        LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
        const InterfaceDefinition* interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);

        propertyCount = SerialPnp_GetListCount(interfaceDef->Properties);

        if ((PropertyName != NULL) && (PropertyValueString != NULL) && (propertyCount > 0))
        {

            LogInfo("Serial Pnp Adapter: Processing property %s. Property update value = %.*s", PropertyName, (int)PropertyValueLen, PropertyValueString);

            IOTHUB_CLIENT_RESULT result = SerialPnp_PropertyHandler(deviceContext, PropertyName, (char*) PropertyValueString);

            if (result == IOTHUB_CLIENT_OK)
            {
                if ((jsonToSend = PnP_CreateReportedProperty(deviceContext->ComponentName, PropertyName, PropertyValueString)) == NULL)
                {
                    LogError("Serial Pnp Adapter: Unable to build reported property response for propertyName=%s, propertyValue=%s",
                        PropertyName, PropertyValueString);
                }
                else
                {
                    const char* jsonToSendStr = STRING_c_str(jsonToSend);
                    size_t jsonToSendStrLen = strlen(jsonToSendStr);

                    if ((iothubClientResult = PnpBridgeClient_SendReportedState((PNP_BRIDGE_CLIENT_HANDLE)userContextCallback, (const unsigned char*)jsonToSendStr, jsonToSendStrLen,
                        NULL, NULL)) != IOTHUB_CLIENT_OK)
                    {
                        LogError("Serial Pnp Adapter: Unable to send reported state for device property=%s, error=%d",
                            PropertyName, iothubClientResult);
                    }
                    else
                    {
                        LogInfo("Serial Pnp Adapter: Sent device information property to IoTHub. propertyName=%s, propertyValue=%s",
                            PropertyName, PropertyValueString);
                    }

                    STRING_delete(jsonToSend);
                }
            }
        }
        else
        {            
			LogError("((PropertyName != NULL) && (PropertyValueString != NULL) && (propertyCount > 0)) was false in SerialPnp_PropertyUpdateHandler()");        
		}

    }
    if (PropertyValueString != NULL)
        free(PropertyValueString);
}

// SerialPnp_SetCommandResponse is a helper that fills out a PNP_CLIENT_COMMAND_RESPONSE
static int SerialPnp_SetCommandResponse(
    unsigned char** CommandResponse,
    size_t* CommandResponseSize,
    const char* ResponseData)
{
    int result = PNP_STATUS_SUCCESS;
    if (ResponseData == NULL)
    {
        LogError("Serial Pnp Adapter: Response Data is empty");
        *CommandResponseSize = 0;
        return result;
    }

    *CommandResponseSize = strlen((char*)ResponseData);
    memset(CommandResponse, 0, sizeof(*CommandResponse));

    // Allocate a copy of the response data to return to the invoker. Caller will free this.
    if (mallocAndStrcpy_s((char**)CommandResponse, (char*)ResponseData) != 0)
    {
        LogError("ENVIRONMENTAL_SENSOR_INTERFACE: Unable to allocate response data");
        result = PNP_STATUS_INTERNAL_ERROR;
    }

    return result;
}

int 
SerialPnp_CommandUpdateHandler(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle,
    const char* CommandName,
    JSON_Value* CommandValue,
    unsigned char** CommandResponse,
    size_t* CommandResponseSize)
{
    PSERIAL_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);
    LIST_ITEM_HANDLE interfaceDefHandle = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
    const InterfaceDefinition* interfaceDef = singlylinkedlist_item_get_value(interfaceDefHandle);
    int commandCount = SerialPnp_GetListCount(interfaceDef->Commands);

    char* response = NULL;
    char* requestData = (char*) json_value_get_string(CommandValue);
    if ((commandCount > 0) && (requestData != NULL))
    {
        SerialPnp_CommandHandler(deviceContext, CommandName, (char*)requestData, &response);
        return SerialPnp_SetCommandResponse(CommandResponse, CommandResponseSize, response);
    }
    return PNP_STATUS_NOT_FOUND;
}

IOTHUB_CLIENT_RESULT
SerialPnp_StartPnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    PSERIAL_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);
    if (NULL == deviceContext)
    {
        LogError("Device context is null, unable to start component");
        return IOTHUB_CLIENT_ERROR;
    }

    // Assign client handle
    deviceContext->ClientHandle = PnpComponentHandleGetClientHandle(PnpComponentHandle);

    PnpComponentHandleSetContext(PnpComponentHandle, deviceContext);

    // Start telemetry thread
    if (ThreadAPI_Create(&deviceContext->TelemetryWorkerHandle, SerialPnp_UartReceiver, deviceContext) != THREADAPI_OK) {
        LogError("ThreadAPI_Create failed");
        return IOTHUB_CLIENT_ERROR;
    }
    return IOTHUB_CLIENT_OK;
}


void SerialPnp_FreeFieldDefinition(
    FieldDefinition* fdef)
{
    if (NULL != fdef->Description)
    {
        free(fdef->Description);
    }

    if (NULL != fdef->DisplayName)
    {
        free(fdef->DisplayName);
    }

    if (NULL != fdef->Name)
    {
        free(fdef->Name);
    }
}

void SerialPnp_FreeEventDefinition(
    SINGLYLINKEDLIST_HANDLE events)
{
    if (NULL == events)
    {
        return;
    }

    LIST_ITEM_HANDLE eventItem = singlylinkedlist_get_head_item(events);
    while (NULL != eventItem)
    {
        EventDefinition* e = (EventDefinition*)singlylinkedlist_item_get_value(eventItem);
        SerialPnp_FreeFieldDefinition(&e->defintion);
        if (NULL != e->Units)
        {
            free(e->Units);
        }
        free(e);
        eventItem = singlylinkedlist_get_next_item(eventItem);
    }
}

void SerialPnp_FreeCommandDefinition(
    SINGLYLINKEDLIST_HANDLE cmds)
{
    if (NULL == cmds)
    {
        return;
    }

    LIST_ITEM_HANDLE cmdItem = singlylinkedlist_get_head_item(cmds);
    while (NULL != cmdItem)
    {
        CommandDefinition* c = (CommandDefinition*)singlylinkedlist_item_get_value(cmdItem);
        SerialPnp_FreeFieldDefinition(&c->defintion);
        free(c);
        cmdItem = singlylinkedlist_get_next_item(cmdItem);
    }
}

void SerialPnp_FreePropertiesDefinition(
    SINGLYLINKEDLIST_HANDLE props)
{
    if (NULL == props)
    {
        return;
    }

    LIST_ITEM_HANDLE propItem = singlylinkedlist_get_head_item(props);
    while (NULL != propItem)
    {
        PropertyDefinition* p = (PropertyDefinition*)singlylinkedlist_item_get_value(propItem);
        SerialPnp_FreeFieldDefinition(&p->defintion);
        if (NULL != p->Units)
        {
            free(p->Units);
        }
        free(p);
        propItem = singlylinkedlist_get_next_item(propItem);
    }
}

IOTHUB_CLIENT_RESULT SerialPnp_StopPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PSERIAL_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);

    if (NULL == deviceContext)
    {
        return IOTHUB_CLIENT_OK;
    }

#ifdef WIN32
    if (NULL != deviceContext->hSerial)
    {
        CloseHandle(deviceContext->hSerial);
    }
    if (NULL != deviceContext->osWriter.hEvent)
    {
        CloseHandle(deviceContext->osWriter.hEvent);
    }
    if (NULL != deviceContext->osReader.hEvent)
    {
        CloseHandle(deviceContext->osReader.hEvent);
    }
#else
    if (0 < deviceContext->hSerial)
    {
        close(deviceContext->hSerial);
    }
#endif
    ThreadAPI_Join(deviceContext->TelemetryWorkerHandle, NULL);
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT SerialPnp_DestroyPnpComponent(
    PNPBRIDGE_COMPONENT_HANDLE PnpComponentHandle)
{
    PSERIAL_DEVICE_CONTEXT deviceContext = PnpComponentHandleGetContext(PnpComponentHandle);

    if (NULL == deviceContext) {
        return IOTHUB_CLIENT_OK;
    }

    if (deviceContext->InterfaceDefinitions)
    {
        LIST_ITEM_HANDLE interfaceItem = singlylinkedlist_get_head_item(deviceContext->InterfaceDefinitions);
        while (NULL != interfaceItem)
        {
            InterfaceDefinition* def = (InterfaceDefinition*)singlylinkedlist_item_get_value(interfaceItem);
            if (NULL != def->Id)
            {
                free(def->Id);
            }
            SerialPnp_FreeEventDefinition(def->Events);
            SerialPnp_FreeCommandDefinition(def->Commands);
            SerialPnp_FreePropertiesDefinition(def->Properties);

            free(def);
            interfaceItem = singlylinkedlist_get_next_item(interfaceItem);
        }
        singlylinkedlist_destroy(deviceContext->InterfaceDefinitions);
    }

    if(deviceContext->ComponentName)
    {
        free(deviceContext->ComponentName);
    }

    free(deviceContext);

    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT
SerialPnp_CreatePnpComponent(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle,
    const char* ComponentName,
    const JSON_Object* AdapterComponentConfig,
    PNPBRIDGE_COMPONENT_HANDLE BridgeComponentHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    IOTHUB_CLIENT_RESULT result = IOTHUB_CLIENT_OK;

    if (strlen(ComponentName) > PNP_MAXIMUM_COMPONENT_LENGTH)
    {
        LogError("ComponentName=%s is too long.  Maximum length is=%d", ComponentName, PNP_MAXIMUM_COMPONENT_LENGTH);
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    // Get device connection information

    const char* port = NULL;
    const char* useComDevInterfaceStr;
    const char* baudRateParam;
    bool useComDeviceInterface = false;

    useComDevInterfaceStr = json_object_dotget_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_SERIALPNP_USEDEFAULT);
    if ((NULL != useComDevInterfaceStr) && (0 == strcmp(useComDevInterfaceStr, "true")))
    {
        useComDeviceInterface = true;
    }

    if (!useComDeviceInterface)
    {
        port = json_object_dotget_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_SERIALPNP_COMPORT);
        if (NULL == port)
        {
            LogError("COM port parameter is missing in configuration");
            result = IOTHUB_CLIENT_INVALID_ARG;
            goto exit;
        }
    }

    baudRateParam = json_object_dotget_string(AdapterComponentConfig, PNP_CONFIG_ADAPTER_SERIALPNP_BAUDRATE);
    if (NULL == baudRateParam)
    {
        LogError("Baud rate parameter is missing in configuration");
        result = IOTHUB_CLIENT_INVALID_ARG;
        goto exit;
    }

    PSERIAL_DEVICE seriaDevice = NULL;
    DWORD baudRate = atoi(baudRateParam);
    if (useComDeviceInterface)
    {
#ifdef WIN32
        if (SerialPnp_FindSerialDevices() != IOTHUB_CLIENT_OK)
#endif
        {
            LogError("Failed to get com port %s", port);
            result = IOTHUB_CLIENT_INVALID_ARG;
            goto exit;
        }

        LIST_ITEM_HANDLE item = singlylinkedlist_get_head_item(SerialDeviceList);
        if (NULL == item)
        {
            LogError("No serial device was found %s", port);
            result = IOTHUB_CLIENT_ERROR;
            goto exit;
        }

        seriaDevice = (PSERIAL_DEVICE)singlylinkedlist_item_get_value(item);
    }

    // Setup serial pnp device context
    LogInfo("Opening com port %s", useComDeviceInterface ? seriaDevice->InterfaceName : port);

    PSERIAL_DEVICE_CONTEXT deviceContext = malloc(sizeof(SERIAL_DEVICE_CONTEXT));
    if (NULL == deviceContext)
    {
        LogError("Error out of memory");
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }
    memset(deviceContext, 0, sizeof(SERIAL_DEVICE_CONTEXT));
    mallocAndStrcpy_s((char**)&deviceContext->ComponentName, ComponentName);
    deviceContext->RxBufferIndex = 0;
    deviceContext->RxEscaped = false;

    deviceContext->CommandLock = Lock_Init();
    deviceContext->CommandResponseWaitLock = Lock_Init();
    deviceContext->CommandResponseWaitCondition = Condition_Init();
    if (NULL == deviceContext->CommandLock ||
        NULL == deviceContext->CommandResponseWaitLock ||
        NULL == deviceContext->CommandResponseWaitCondition)
    {
        if (NULL != deviceContext->CommandLock)
        {
            Lock_Deinit(deviceContext->CommandLock);
        }
        if (NULL != deviceContext->CommandResponseWaitLock)
        {
            Lock_Deinit(deviceContext->CommandResponseWaitLock);
        }
        if (NULL != deviceContext->CommandResponseWaitCondition)
        {
            Lock_Deinit(deviceContext->CommandResponseWaitCondition);
        }
        result = IOTHUB_CLIENT_ERROR;
        goto exit;
    }
    deviceContext->InterfaceDefinitions = singlylinkedlist_create();

    // Open device and store handle in device context
    result = SerialPnp_OpenDevice(useComDeviceInterface ? seriaDevice->InterfaceName : port, baudRate, deviceContext);

    // Retrieve device descriptor and populate supported interface configurations
    if (THREADAPI_OK != ThreadAPI_Create(&deviceContext->SerialDeviceWorker, SerialPnp_ParseInterfaceConfig, deviceContext))
    {
        LogError("ThreadAPI_Create failed");
    }

    // Assign client handle
    if (PnpComponentHandleGetIoTType(BridgeComponentHandle) == PNP_BRIDGE_IOT_TYPE_DEVICE)
    {
        deviceContext->ClientType = PNP_BRIDGE_IOT_TYPE_DEVICE;
    }
    else
    {
        deviceContext->ClientType = PNP_BRIDGE_IOT_TYPE_RUNTIME_MODULE;
    }

    PnpComponentHandleSetContext(BridgeComponentHandle, deviceContext);
    PnpComponentHandleSetPropertyUpdateCallback(BridgeComponentHandle, SerialPnp_PropertyUpdateHandler);
    PnpComponentHandleSetCommandCallback(BridgeComponentHandle, SerialPnp_CommandUpdateHandler);

exit:
    if (result != IOTHUB_CLIENT_OK)
    {
        SerialPnp_DestroyPnpComponent(BridgeComponentHandle);
    }

    return result;
}

IOTHUB_CLIENT_RESULT SerialPnp_CreatePnpAdapter(
    const JSON_Object* AdapterGlobalConfig,
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterGlobalConfig);
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return IOTHUB_CLIENT_OK;
}

IOTHUB_CLIENT_RESULT SerialPnp_DestroyPnpAdapter(
    PNPBRIDGE_ADAPTER_HANDLE AdapterHandle)
{
    AZURE_UNREFERENCED_PARAMETER(AdapterHandle);
    return IOTHUB_CLIENT_OK;
}

PNP_ADAPTER SerialPnpInterface = {
    .identity = "serial-pnp-interface",
    .createAdapter = SerialPnp_CreatePnpAdapter,
    .createPnpComponent = SerialPnp_CreatePnpComponent,
    .startPnpComponent = SerialPnp_StartPnpComponent,
    .stopPnpComponent = SerialPnp_StopPnpComponent,
    .destroyPnpComponent = SerialPnp_DestroyPnpComponent,
    .destroyAdapter = SerialPnp_DestroyPnpAdapter
};

