#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../helpers/string_manipulation.h"

void curlGlobalInit() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t curlStaticDataReadCallback(void *contents, size_t size, size_t nmemb, void *userp) {

    size_t realsize = size * nmemb;
    
    struct CURL_Session_Data *session_data = (struct CURL_Session_Data*)userp;

    memcpy(*(session_data->callbackData), (char*)contents, nmemb);
    session_data->callbackDataLength = (int)nmemb;

    fprintf(stdout,"\n   Size: %d", session_data->callbackDataLength);
    fprintf(stdout,"\n   %s", (char*)*(session_data->callbackData));
    fprintf(stdout, "\n");

  return nmemb; // realsize;
}

CURL_Session_Data * curlStaticInit(char *username, char *password, char *basePath, int EnableVerify, size_t (*callbackFunction)(), long verboseOutput) {
  
  CURL *static_handle;
  static_handle = curl_easy_init();

  #define CALLBACK_DATA_BUFFER 10000
  static char *callback_data;
  callback_data = (char *)malloc(sizeof(char)*CALLBACK_DATA_BUFFER);
  
  if(callback_data == NULL) {
    /* out of memory! */ 
    printf("not enough memory for cURL callback data buffer (malloc returned NULL)\n");
    return 0;
  }

  // build user:password string for cURL
  char usrpwd_build[256] = "";
  strcat(usrpwd_build, username); 
  strcat(usrpwd_build, ":"); 
  strcat(usrpwd_build, password);

  char* usrpwd = Str_Trim(usrpwd_build).strPtr;

  // Set session data values
  #define initArraySize 10
  int structInitSizes[initArraySize] = {
      8,              // curl session pointer
      8,              //username pointer
      sizeof(int),    //username size
      8,              //password pointer
      sizeof(int),    //password size
      8,              //basepath pointer
      sizeof(int),    //basepath size
      sizeof(char**), //callbackData pointer
      sizeof(int),    //callbackData size
      sizeof(int)     //callbackBuffer Size
  };

  int sessionMemSize = 0;
  for (int i = 0; i<initArraySize; i++) {
    sessionMemSize = sessionMemSize + structInitSizes[i];
  }

  static CURL_Session_Data *session_data; 
  session_data = malloc(sessionMemSize);
  
    session_data->curlHandle = static_handle;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);
    session_data->callbackData = &callback_data;
    session_data->callbackDataLength = 0;
    session_data->callbackBufferSize = CALLBACK_DATA_BUFFER;


// apply curl settings with libcurl calls
  curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, callbackFunction);
  curl_easy_setopt(static_handle, CURLOPT_WRITEDATA, (void *)session_data);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_VERBOSE, verboseOutput);

// test curl configuration
  CURLcode res;
  res = curlStaticGet(session_data, "/status");

   if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      fprintf(stdout, "\n    ERROR: Initialization of cURL session failed!  (test call failed, check inputs)");
   }
      

  return session_data;

}

CURLcode curlStaticGet(CURL_Session_Data *session_data, char *endpoint) {
  
  CURL *static_handle = session_data->curlHandle;

  char* startPtr = *(session_data->callbackData);
  char nullChar[1] = "\000";
  for (int i=0;i<CALLBACK_DATA_BUFFER;i++) {
    // set memory to \000
    memcpy(startPtr + i, &nullChar, sizeof(char));
  }

  session_data->callbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  fprintf(stdout, "\n Endpoint: %s", full_endpoint);

  curl_easy_setopt(static_handle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);

  return curl_easy_perform(static_handle);

}

CURLcode curlStaticPost(CURL_Session_Data *session_data, char *endpoint, char *postData) {
  
  CURL *static_handle = session_data->curlHandle;

  char* startPtr = *(session_data->callbackData);
  char nullChar[1] = "\000";
  for (int i=0;i<CALLBACK_DATA_BUFFER;i++) {
    // set memory to \000
    memcpy(startPtr + i, &nullChar, sizeof(char));
  }

  session_data->callbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  fprintf(stdout, "\n Endpoint: %s", full_endpoint);

  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
  curl_easy_setopt(static_handle, CURLOPT_POST, 1);
  curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, postData);

  return curl_easy_perform(static_handle);

}

void curlStaticCleanup(CURL_Session_Data *session_data) {
    
    CURL *static_handle = session_data->curlHandle;
    
    free(*(session_data->callbackData));
    free(session_data);

    curl_easy_cleanup(static_handle);
}

void curlGlobalCleanup() {
     curl_global_cleanup();
}

