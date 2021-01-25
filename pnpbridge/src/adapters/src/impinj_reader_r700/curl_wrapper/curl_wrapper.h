#pragma once
#include <curl/curl.h>
#include <stdint.h>

#define VERBOSE_OUTPUT_OFF 0
#define VERBOSE_OUTPUT_ON 1

#define VERIFY_CERTS_OFF 0
#define VERIFY_CERTS_ON 1

typedef struct CURL_Static_Session_Data {
  CURL *curlHandle;
  char *username;
  int usernameLength;
  char *password;
  int passwordLength;
  char *basePath;
  int basePathLength;
  char **callbackData;
  int callbackDataLength;
  int callbackBufferSize;
  } CURL_Static_Session_Data;

typedef struct CURL_Stream_Session_Data {
  CURL *curlHandle;
  char *username;
  int usernameLength;
  char *password;
  int passwordLength;
  char *basePath;
  int basePathLength;
  char *dataBuffer;
  int dataBufferSize;
  int bufferReadIndex;
  int64_t bufferReadCounter;
  int bufferWriteIndex;
  int64_t bufferWriteCounter;
  } CURL_Stream_Session_Data;

void 
curlGlobalInit();

void
curlStreamBufferReadout(
  CURL_Stream_Session_Data * session_data
  );

size_t 
curlStaticDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
  );

size_t 
curlStreamDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
  );

size_t 
curlDummyCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
 );

CURL_Static_Session_Data * 
curlStaticInit(
  char *username, 
  char *password, 
  char *basePath, 
  int EnableVerify, 
  long verboseOutput
  );

CURL_Stream_Session_Data * 
curlStreamInit(
  char *username, 
  char *password, 
  char *basePath, 
  int EnableVerify, 
  long verboseOutput
  );

char** 
curlStaticGet(
  CURL_Static_Session_Data *session_data, 
  char *endpoint
  );

char** 
curlStaticPost(
  CURL_Static_Session_Data *session_data, 
  char *endpoint, 
  char *postData
  );

void 
curlStaticCleanup(
  CURL_Static_Session_Data *session_data
  );

void 
curlStreamCleanup(
  CURL_Stream_Session_Data *session_data
  );

void 
curlGlobalCleanup();