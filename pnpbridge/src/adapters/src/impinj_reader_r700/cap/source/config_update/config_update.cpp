#include <iostream>
#include <fstream>
#include "../../../../../../../deps/azure-iot-sdk-c-pnp/deps/parson/parson.h"
#include <string.h>
#include "./../../../curl_wrapper/curl_wrapper.h"

using namespace std;

// Current values: 
#define CURRENT_ROOT_INTERFACE_MODEL_ID "dtmi:impinj:FixedReader;12"
#define CURRENT_COMPONENT_NAME "R700"
#define CURRENT_ADAPTER_ID "impinj-reader-r700"
#define CURRENT_COMMENT "Impinj Reader Device (R700)"

// JSON field names
#define PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING "pnp_bridge_connection_parameters.root_interface_model_id"
#define PNP_CONFIG_BRIDGE_COMPONENT_NAME "pnp_bridge_component_name"
#define PNP_CONFIG_BRIDGE_ADAPTER_ID "pnp_bridge_adapter_id"
#define PNP_CONFIG_BRIDGE_COMMENT "_comment"
#define PNP_CONFIG_DEVICES "pnp_bridge_interface_components"

string jsonObjectDotGetString(
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

string parseMacAddress(string systemNetworkInterfaces) {
  int interfaceCount;
  JSON_Value* jsonValue;
  JSON_Object* jsonObject; 
  JSON_Array* jsonArray;
  jsonValue = json_parse_string(systemNetworkInterfaces.c_str());
  jsonObject = json_value_get_object(jsonValue);
  jsonArray = json_object_get_array(jsonObject, "");
  interfaceCount = json_array_get_count(jsonArray);
  JSON_Object* jsonInterfaceObjects[interfaceCount];

  // get/set device object pnp component parameters
  for (int i = 0; i < interfaceCount; i++) {
    jsonInterfaceObjects[i] = json_array_get_object(jsonArray, i);
    cout << endl << "Interface" << i + 1 << ": " << jsonObjectDotGetString(jsonInterfaceObjects[i], "interfaceId") << endl;
    cout << "   interfaceName: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "interfaceName") << endl;
    cout << "   intefaceType: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "intefaceType") << endl;
    cout << "   hardwareAddress: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "hardwareAddress") << endl;
  }
  return (string)jsonObjectDotGetString(jsonInterfaceObjects[0], "hardwareAddress");
}

string generateDeviceName(string prefix) {
  char * res;
  int httpStatus;
  JSON_Value* jsonValue;
  string deviceName;
  string macAddress;

  curlGlobalInit();
 
  CURL_Static_Session_Data *static_session = curlStaticInit("root", "impinj", "https://192.168.1.14/api/v1", Session_Static, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  // res = curlStaticGet(static_session, "/system/network/interfaces", &httpStatus);
  // fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  curlStaticCleanup(static_session);

  curlGlobalCleanup();

  // macAddress = parseMacAddress(string(res));

  return prefix + macAddress;

}

int main(int argc, char* argv[]) {

  if (argc != 3) {
    cout << "Incorrect usage: must specify intput and output files as arguements." << endl;
    cout << "     Argument 1: path to input file" << endl;
    cout << "     Argument 2: path of output file" << endl;
    return 0;

  }

  else {
    cout << "config file: " << argv[1] << endl;
  }


  // Check if file exists
  string line;
  ifstream myfile (argv[1]);
  if (myfile.is_open())
  {
    myfile.close();

    // Serialize the json file using parson
    JSON_Value* config;

    config = json_parse_file(argv[1]);
    if (config == NULL) {
      cout << "Failed to parse the config file. Please validate the JSON file in a JSON vailidator" << endl;
    }

    else {
      
      cout << "Successfully loaded config.json." << endl; 

      cout << json_serialize_to_string_pretty(config) << endl; 
      
      JSON_Object* jsonRootObject = json_value_get_object(config);

      string rootInterfaceModelId = jsonObjectDotGetString(jsonRootObject, PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING);
      cout << endl << "Old root model ID: " << rootInterfaceModelId << endl;
      json_object_dotset_string(jsonRootObject, PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING, CURRENT_ROOT_INTERFACE_MODEL_ID);
      cout << ">> New root model ID: " << CURRENT_ROOT_INTERFACE_MODEL_ID << endl;

      JSON_Array* devices = json_object_dotget_array(jsonRootObject, PNP_CONFIG_DEVICES);
      int deviceCount = json_array_get_count(devices);
      cout << endl << "# of devices: " << deviceCount << endl;
      JSON_Object* jsonDeviceObjects[deviceCount];

      // get/set device object pnp component parameters
      for (int i = 0; i < deviceCount; i++) {
        jsonDeviceObjects[i] = json_array_get_object(devices, i);
        cout << endl << "Old Device" << i + 1 << " Values: " << endl;
        cout << "   Comment: " << jsonObjectDotGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_COMMENT) << endl;
        cout << "   Component Name: " << jsonObjectDotGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_COMPONENT_NAME) << endl;
        cout << "   Adapter ID: " << jsonObjectDotGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_ADAPTER_ID) << endl;
        json_object_dotset_string(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_COMMENT, CURRENT_COMMENT);
        json_object_dotset_string(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_COMPONENT_NAME, CURRENT_COMPONENT_NAME);
        json_object_dotset_string(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_ADAPTER_ID, CURRENT_ADAPTER_ID);
        cout << endl << ">> New Device" << i + 1 << " Values: " << endl;
        cout << "   >> Comment: " << CURRENT_COMMENT << endl;
        cout << "   >> Component Name: " << CURRENT_COMPONENT_NAME << endl;
        cout << "   >> Adapter ID: " << CURRENT_ADAPTER_ID << endl;
      }

      cout << json_serialize_to_string_pretty(config) << endl; 
      json_serialize_to_file_pretty(config, argv[2]);

    }

    cout << "Device Name: " << generateDeviceName("impinj-");

  }

  else cout << "Unable to open file"; 

  return 0;
}