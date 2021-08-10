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
#define PNP_CONFIG_DEVICE_ID_DOT_STRING "pnp_bridge_connection_parameters.dps_parameters.device_id"
#define PNP_CONFIG_BRIDGE_COMPONENT_NAME "pnp_bridge_component_name"
#define PNP_CONFIG_BRIDGE_ADAPTER_ID "pnp_bridge_adapter_id"
#define PNP_CONFIG_BRIDGE_COMMENT "_comment"
#define PNP_CONFIG_DEVICES "pnp_bridge_interface_components"
#define PNP_CONFIG_BRIDGE_HOSTNAME "pnp_bridge_adapter_config.hostname"

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
  string macAddress = "";
  string fixedJson = "{\"networkInterfaces\": " + systemNetworkInterfaces + "}";  // need to add owning object name to raw JSON response so API can get object ref by name

  jsonValue = json_parse_string(fixedJson.c_str());
  jsonObject = json_value_get_object(jsonValue);
  jsonArray = json_object_get_array(jsonObject, "networkInterfaces");
  interfaceCount = json_array_get_count(jsonArray);
  JSON_Object* jsonInterfaceObjects[interfaceCount];

  // get/set device object pnp component parameters
  for (int i = 0; i < interfaceCount; i++) {
    jsonInterfaceObjects[i] = json_array_get_object(jsonArray, i);
    const JSON_Value * tempVal = json_object_get_wrapping_value(jsonInterfaceObjects[i]);
    cout << endl << "interfaceId: " << json_object_dotget_number(jsonInterfaceObjects[i], "interfaceId") << endl;
    cout << "   interfaceName: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "interfaceName") << endl;
    cout << "   interfaceType: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "interfaceType") << endl;
    cout << "   hardwareAddress: " << jsonObjectDotGetString(jsonInterfaceObjects[i], "hardwareAddress") << endl;
  }

  macAddress = jsonObjectDotGetString(jsonInterfaceObjects[0], "hardwareAddress");

  return macAddress;
}

void findAndReplaceAll(std::string & data, std::string toSearch, std::string replaceStr)
{
    // Get the first occurrence
    size_t pos = data.find(toSearch);
    // Repeat till end is reached
    while( pos != std::string::npos)
    {
        // Replace this occurrence of Sub String
        data.replace(pos, toSearch.size(), replaceStr);
        // Get the next occurrence from the current position
        pos =data.find(toSearch, pos + replaceStr.size());
    }
}

string generateDeviceName(string prefix, string host) {
  char * res;
  int httpStatus;
  JSON_Value* jsonValue;
  string deviceName;
  string macAddress;

  string baseUrl = "https://" + host + "/api/v1";

  curlGlobalInit();
 
  CURL_Static_Session_Data *static_session = curlStaticInit("root", "impinj", baseUrl.c_str(), Session_Static, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  res = curlStaticGet(static_session, "/system/network/interfaces", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  curlStaticCleanup(static_session);

  curlGlobalCleanup();

  macAddress = parseMacAddress(string(res));

  findAndReplaceAll(macAddress, ":", "-");

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
      
      cout << endl << "Successfully loaded config.json." << endl;  
      
      JSON_Object* jsonRootObject = json_value_get_object(config);

      string rootInterfaceModelId = jsonObjectDotGetString(jsonRootObject, PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING);
      cout << endl << "Old root model ID: " << rootInterfaceModelId << endl;

      json_object_dotset_string(jsonRootObject, PNP_CONFIG_ROOT_INTERFACE_MODEL_ID_DOT_STRING, CURRENT_ROOT_INTERFACE_MODEL_ID);
      cout << ">> New root model ID: " << CURRENT_ROOT_INTERFACE_MODEL_ID << endl;

      string deviceId = jsonObjectDotGetString(jsonRootObject, PNP_CONFIG_DEVICE_ID_DOT_STRING);
      cout << endl << "Old Device ID: " << deviceId << endl;

      if (deviceId == "") cout << "(will update with auto-generated name)" << endl;

      JSON_Array* devices = json_object_dotget_array(jsonRootObject, PNP_CONFIG_DEVICES);
      int deviceCount = json_array_get_count(devices);
      cout << endl << "# of devices: " << deviceCount << endl;
      JSON_Object* jsonDeviceObjects[deviceCount];

      string hostname = "";
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

        hostname = jsonObjectDotGetString(jsonDeviceObjects[i], PNP_CONFIG_BRIDGE_HOSTNAME);
        cout << endl << " Hostname: " << hostname << endl << endl;
      }

      if (deviceId == "") {
        string newDeviceId = generateDeviceName("impinj-", hostname);
        json_object_dotset_string(jsonRootObject, PNP_CONFIG_DEVICE_ID_DOT_STRING, newDeviceId.c_str());
        cout << endl << ">> New Device ID: " << newDeviceId << endl;
      }

      cout << endl << json_serialize_to_string_pretty(config) << endl; 
      json_serialize_to_file_pretty(config, argv[2]);

    }
    return 0;

  }

  else cout << "Unable to open file"; 

  return 0;
}