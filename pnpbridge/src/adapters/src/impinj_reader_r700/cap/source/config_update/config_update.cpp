#include <iostream>
#include <fstream>
#include "../../../../../../../deps/azure-iot-sdk-c-pnp/deps/parson/parson.h"
#include <string.h>

using namespace std;

#define PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING "pnp_bridge_connection_parameters.root_interface_model_id"
#define PNP_CONFIG_BRIDGE_COMPONENT_NAME "pnp_bridge_component_name"
#define PNP_CONFIG_BRIDGE_ADAPTER_ID "pnp_bridge_adapter_id"
#define PNP_CONFIG_DEVICES "pnp_bridge_interface_components"

string jsonObjectDogGetString(
  JSON_Object * jsonObject, 
  string dotString)
  {
    string jsonObjectString;
    const char * jsonObjectStringConst = json_object_dotget_string(jsonObject, dotString.c_str());
    if (NULL == jsonObjectStringConst) {
      cout << "\n" << dotString << " is missing.";
    }

    jsonObjectString = string(jsonObjectStringConst);
    return jsonObjectStringConst;
  }

int main() {

  // Check if file exists
  string line;
  ifstream myfile ("../test/config.json");
  if (myfile.is_open())
  {
    // Serialize the json file using parson
    JSON_Value* config;

    config = json_parse_file("../test/config.json");
    if (config == NULL) {
      cout << "Failed to parse the config file. Please validate the JSON file in a JSON vailidator" << endl;
    }

    else {
      cout << "Successfully loaded config.json." << endl;  
      
      JSON_Object* jsonRootObject = json_value_get_object(config);

      string rootInterfaceModelId = jsonObjectDogGetString(jsonRootObject, PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING);
      cout << PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING << ": " << rootInterfaceModelId << endl;

      JSON_Array* devices = json_object_dotget_array(jsonRootObject, PNP_CONFIG_DEVICES);
      int deviceCount = json_array_get_count(devices);
      cout << "# of devices: " << deviceCount << endl;
      JSON_Object* jsonDeviceObjects[deviceCount];
      
      struct PnpBridgeDeviceParameters {
        string componentName;
        string adapterId;
      };

      PnpBridgeDeviceParameters deviceParamsArray[deviceCount];

      for (int i = 0; i < deviceCount; i++) {
        jsonDeviceObjects[i] = json_array_get_object(devices, i);
        deviceParamsArray[i].componentName = jsonObjectDogGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_COMPONENT_NAME);
        deviceParamsArray[i].adapterId = jsonObjectDogGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_ADAPTER_ID);
        cout << "Device " << i + 1 << ": " << endl;
        cout << "   Component Name: " << deviceParamsArray[i].componentName << endl;
        cout << "   Adapter ID: " << deviceParamsArray[i].adapterId << endl;
      }
  
    cout.flush();

    }
    myfile.close();
  }

  else cout << "Unable to open file"; 

  return 0;
}