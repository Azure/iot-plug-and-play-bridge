#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../helpers/string_manipulation.h"

void 
curlGlobalInit() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t 
curlStaticDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
  ) 
  {

    size_t realsize = size * nmemb;
    
    struct CURL_Static_Session_Data *session_data = (struct CURL_Static_Session_Data*)userp;

    memcpy(*(session_data->callbackData), (char*)contents, nmemb);
    session_data->callbackDataLength = (int)nmemb;

    /* DEBUG - print out response */
    // fprintf(stdout,"\n   Size: %d", session_data->callbackDataLength);
    // fprintf(stdout,"\n   %s", (char*)*(session_data->callbackData));
    // fprintf(stdout, "\n");

  return nmemb; // realsize;
}

char * curlReadStreamBufferChunk(
  CURL_Stream_Session_Data *session_data
)  {
      if (session_data->bufferReadIndex == session_data->bufferWriteIndex) {
        return NULL;
      }
      
      char * dataChunk = (session_data->dataBuffer[session_data->bufferReadIndex]);
      session_data->bufferReadIndex = session_data->bufferReadIndex + strlen(dataChunk) - 1;

      fprintf(stdout, "\n   POST READ - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);
      return dataChunk;
}

size_t 
curlDummyCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
 ) {

   struct CURL_Stream_Session_Data *session_data = (struct CURL_Stream_Session_Data*)userp;

   fprintf(stdout, "\nRESPONSE: %s", (char*)contents);
   return nmemb;
 }

size_t 
curlStreamDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
 ) {
    
    size_t realsize = size * nmemb;
    
    struct CURL_Stream_Session_Data *session_data = (struct CURL_Stream_Session_Data*)userp;

    if (((session_data->bufferWriteIndex + 1) % session_data->dataBufferSize) == session_data->dataBufferSize) {
      fprintf(stdout, "  ERROR: Stream data buffer overflow.");
      return -1;
    }

    int spaceUntilWrap = session_data->dataBufferSize-session_data->bufferWriteIndex;

    if (nmemb > spaceUntilWrap - 1) {  // wrap around end of buffer
        memcpy(*(session_data->dataBuffer) + session_data->bufferWriteIndex, contents, spaceUntilWrap);
        memcpy(*(session_data->dataBuffer), contents + nmemb, nmemb - spaceUntilWrap);
        session_data->bufferWriteIndex = nmemb - spaceUntilWrap;
    }
    else {
        memcpy(*(session_data->dataBuffer) + session_data->bufferWriteIndex, contents, nmemb);
        // session_data->dataBuffer[session_data->bufferWriteIndex] = contents;
        session_data->bufferWriteIndex = (session_data->bufferWriteIndex + (int)nmemb);
    }
    // memcpy(*(session_data->dataBuffer), (char*)contents, nmemb);

    session_data->dataBuffer[session_data->bufferWriteIndex] = '\000';
    session_data->bufferWriteIndex = (session_data->bufferWriteIndex + 1) % session_data->dataBufferSize;

    fprintf(stdout, "\n   POST WRITE - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);


    char * streamDataChunk = curlReadStreamBufferChunk(session_data);

    
    fprintf(stdout, "\n  StreamDataLength: %d", (int)strlen(streamDataChunk));
    fprintf(stdout, "\n  StreamData: %s", streamDataChunk);
    

    /* DEBUG - print out response */
    // fprintf(stdout,"\n   Size: %d", session_data->callbackDataLength);
    // fprintf(stdout,"\n   %s", (char*)*(session_data->callbackData));
    // fprintf(stdout, "\n");

  return nmemb; // realsize;
 }

CURL_Static_Session_Data * 
curlStaticInit(
  char *username,                        
  char *password, 
  char *basePath, 
  int EnableVerify,  
  long verboseOutput
  ) 
  {
  
  CURL *static_handle;
  static_handle = curl_easy_init();

  #define STATIC_CALLBACK_DATA_BUFFER_SIZE 10000
  static char *callback_data;
  callback_data = (char *)malloc(sizeof(char)*STATIC_CALLBACK_DATA_BUFFER_SIZE);
  
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
  #define staticInitArraySize 10
  int structInitSizes[staticInitArraySize] = {
      sizeof(CURL*),  //curl session pointer
      sizeof(int*),   //username pointer
      sizeof(int),    //username size
      sizeof(int*),   //password pointer
      sizeof(int),    //password size
      sizeof(int*),   //basepath pointer
      sizeof(int),    //basepath size
      sizeof(char**), //callbackData pointer
      sizeof(int),    //callbackData size
      sizeof(int)     //callbackBuffer Size
  };

  CURL_Static_Session_Data *session_data = malloc(sizeof(CURL_Static_Session_Data));
  
    session_data->curlHandle = static_handle;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);
    session_data->callbackData = &callback_data;
    session_data->callbackDataLength = 0;
    session_data->callbackBufferSize = STATIC_CALLBACK_DATA_BUFFER_SIZE;


// apply curl settings with libcurl calls
  curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, curlStaticDataReadCallback);
  curl_easy_setopt(static_handle, CURLOPT_WRITEDATA, (void *)session_data);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_VERBOSE, verboseOutput);

// test curl configuration
  char** response;

  response = curlStaticGet(session_data, "/status");
      
  fprintf(stdout, "\nInitialization test Call: %s\n", *response);

  return session_data;

}

CURL_Stream_Session_Data * curlStreamInit(
  char *username,                        
  char *password, 
  char *basePath, 
  int EnableVerify,  
  long verboseOutput
  ) 
  {
  
  CURL *stream_handle;
  stream_handle = curl_easy_init();

  #define STREAM_DATA_BUFFER_SIZE 1000
  char *streamBuffer;
  streamBuffer = (char *)malloc(sizeof(char)*STREAM_DATA_BUFFER_SIZE);
  
  if(streamBuffer == NULL) {
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
  #define streamInitArraySize 11
  int structInitSizes[streamInitArraySize] = {
      sizeof(CURL*),  // curl session pointer
      sizeof(int*),   //username pointer
      sizeof(int),    //username size
      sizeof(int*),   //password pointer
      sizeof(int),    //password size
      sizeof(int*),   //basepath pointer
      sizeof(int),    //basepath size
      sizeof(char**), //dataBuffer handle
      sizeof(int),    //dataBufferSize
      sizeof(int),    //bufferReadIndex
      sizeof(int)     //bufferWriteIndex
  };


    CURL_Stream_Session_Data *session_data = malloc(sizeof(CURL_Stream_Session_Data));
  
    session_data->curlHandle = stream_handle;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);
    session_data->dataBuffer = &streamBuffer;
    session_data->dataBufferSize = STREAM_DATA_BUFFER_SIZE-10;
    session_data->bufferReadIndex = 0;
    session_data->bufferWriteIndex = 0;


// apply curl settings with libcurl calls
  curl_easy_setopt(stream_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(stream_handle, CURLOPT_WRITEFUNCTION, curlDummyCallback);
  curl_easy_setopt(stream_handle, CURLOPT_WRITEDATA, (void *)session_data);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_VERBOSE, verboseOutput);

// test curl configuration (can't do this for stream)
  // char** response;

  // response = curlStaticGet(session_data, "/status");
      
  // fprintf(stdout, "\nInitialization test Call: %s\n", *response);

  return session_data;
}


char** 
curlStaticGet(
  CURL_Static_Session_Data *session_data, 
  char *endpoint
  ) 
  {
  
  CURL *static_handle = session_data->curlHandle;

  char* startPtr = *(session_data->callbackData);
  // for (int i=0;i<STATIC_CALLBACK_DATA_BUFFER_SIZE;i++) {
  //   // set memory to \000
  //   memcpy(startPtr + i, '\0', sizeof(char));
  // }

  memset(startPtr, '\0', STATIC_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));

  session_data->callbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  fprintf(stdout, " GET Endpoint: %s\n", full_endpoint);

  curl_easy_setopt(static_handle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);

  CURLcode res;

  res = curl_easy_perform(static_handle);

  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed in curlStaticGet: %s\n",
      curl_easy_strerror(res));
  }

  return session_data->callbackData;
}

 char** 
 curlStaticPost(
   CURL_Static_Session_Data *session_data, 
   char *endpoint, 
   char *postData
   ) {
  
  CURL *static_handle = session_data->curlHandle;

  // re-initialize callback data buffer
  char* startPtr = *(session_data->callbackData);
  // for (int i=0;i<STATIC_CALLBACK_DATA_BUFFER_SIZE;i++) {
  //   // set memory to \000
  //   memcpy(startPtr + i, &nullChar, sizeof(char));
  // }
  
  memset(startPtr, '\0', STATIC_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
  
  session_data->callbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  fprintf(stdout, " POST Endpoint: %s\n", full_endpoint);

  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
  curl_easy_setopt(static_handle, CURLOPT_POST, 1);
  curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, postData);

  CURLcode res;

  res = curl_easy_perform(static_handle);

  if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed in curlStaticPost: %s\n",
        curl_easy_strerror(res));
  }

  return session_data->callbackData;
}

void 
curlStaticCleanup(
  CURL_Static_Session_Data *session_data
  ) {
    curl_easy_cleanup(session_data->curlHandle);
    free(*(session_data->callbackData));
    free(session_data);
  }

void 
curlStreamCleanup(
  CURL_Stream_Session_Data *session_data
  ) {
    curl_easy_cleanup(session_data->curlHandle);
    free(*(session_data->dataBuffer));
    free(session_data);
  }


void 
curlGlobalCleanup() {
     curl_global_cleanup();
}

