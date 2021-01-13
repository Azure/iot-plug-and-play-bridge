#pragma once
#include <curl/curl.h>

typedef struct CURL_Session_Data {
  char *username;
  char *password;
  CURL *curlHandle;
  
} CURL_Session_Data;

void curlGlobalInit();

size_t curlDataReadCallback(void *contents, size_t size, size_t nmemb, void *userp);

CURL * curlStaticInit(char *username, char *password, int EnableVerify, size_t (*callbackFunction)());

CURLcode curlStaticGet(CURL *static_handle, char *url);

void curlStaticCleanup(CURL *static_handle);

void curlGlobalCleanup();