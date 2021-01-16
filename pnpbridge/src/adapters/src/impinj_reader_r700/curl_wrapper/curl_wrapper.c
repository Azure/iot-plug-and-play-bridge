#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Str_Trim_Data Str_Trim(char *orig_str) {
//   Str_Trim_Data *output;
  
//   int strLength = (int)strlen(orig_str);
//   char trimmed_str[strLength] = "";
//   memcpy(trimmed_str, &orig_str, strLength);
//   trimmed_str[strLength]='\0';

//   output->string = trimmed_str;
//   output->strLength = strLength;

//   return *output;
// }

void curlGlobalInit() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t curlDataReadCallback(void *contents, size_t size, size_t nmemb, void *userp) {

    fprintf(stdout,"\n   Size: %d", (int)nmemb);
    fprintf(stdout,"\n   %s", (char *)contents);
 
  return nmemb; // realsize;
}

CURL_Session_Data * curlStaticInit(char *username, char *password, char *basePath, int EnableVerify, size_t (*callbackFunction)(), long verboseOutput) {
  
  CURL *static_handle;
  static_handle = curl_easy_init();


  // build user:password string for cURL
  char usrpwd[256] = "";
  strcat(usrpwd, username); 
  strcat(usrpwd, ":"); 
  strcat(usrpwd, password);

  // Set session data values
  #define initArraySize 7
  int structInitSizes[initArraySize] = {
      8,              // curl session pointer
      8,              //username pointer
      sizeof(int),    //username size
      8,              //password pointer
      sizeof(int),    //password size
      8,              //basepath pointer
      sizeof(int)     //basepath size
  };

  int sessionMemSize = 0;
  for (int i = 0; i<initArraySize; i++) {
    sessionMemSize = sessionMemSize + structInitSizes[i];
  }

  CURL_Session_Data *session_data = malloc(sessionMemSize);
  
    session_data->curlHandle = static_handle;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);

  // printf("%s", usrpwd);

  curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, callbackFunction);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_VERBOSE, verboseOutput);

  return session_data;

}

CURLcode curlStaticGet(CURL_Session_Data *session_data, char *endpoint) {
  
  CURL *static_handle = session_data->curlHandle;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  curl_easy_setopt(static_handle, CURLOPT_URL, fullurl);

  return curl_easy_perform(static_handle);

}

void curlStaticCleanup(CURL_Session_Data *session_data) {
    
    CURL *static_handle = session_data->curlHandle;
    
    free(session_data);

    curl_easy_cleanup(static_handle);
}

void curlGlobalCleanup() {
     curl_global_cleanup();
}

