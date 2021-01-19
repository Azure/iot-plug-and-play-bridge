#pragma once
#include <curl/curl.h>

#define VERBOSE_OUTPUT_OFF 0
#define VERBOSE_OUTPUT_ON 1

#define VERIFY_CERTS_OFF 0
#define VERIFY_CERTS_ON 1

typedef struct CURL_Session_Data {
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
  } CURL_Session_Data;

void curlGlobalInit();

size_t curlStaticDataReadCallback(void *contents, size_t size, size_t nmemb, void *userp);

CURL_Session_Data * curlStaticInit(char *username, char *password, char *basePath, int EnableVerify, size_t (*callbackFunction)(), long verboseOutput);

CURLcode curlStaticGet(CURL_Session_Data *session_data, char *url);

void curlStaticCleanup(CURL_Session_Data *session_data);

void curlGlobalCleanup();