#pragma once
#include <curl/curl.h>

typedef struct CURL_PARAMS {
  char url[256];
  char username[100];
  char password[100];

} CURL_PARAMS;

size_t DataReadCallback(void *contents, size_t size, size_t nmemb, void *userp);