#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>

void curlGlobalInit() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t curlDataReadCallback(void *contents, size_t size, size_t nmemb, void *userp) {

    fprintf(stdout,"\n   Size: %d", (int)nmemb);
    fprintf(stdout,"\n   %s", (char *)contents);
 
  return nmemb; // realsize;
}

CURL * curlStaticInit(char *username, char *password, int EnableVerify, size_t (*callbackFunction)()) {
  
  CURL *static_handle;
  static_handle = curl_easy_init();

  char buildString[100] = "";
  strcat(buildString, username); 
  strcat(buildString, ":"); 
  strcat(buildString, password);
  int strLength = (int)strlen(buildString);
  char usrpwd[strLength];

  memcpy(usrpwd, &buildString, strLength);
  usrpwd[strLength]='\0';

  // printf("%s", usrpwd);

  curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, callbackFunction);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);

  return static_handle;

}

CURLcode curlStaticGet(CURL *static_handle, char *url) {
  
  curl_easy_setopt(static_handle, CURLOPT_URL, url);

  return curl_easy_perform(static_handle);

}

void curlStaticCleanup(CURL *static_handle) {
   curl_easy_cleanup(static_handle);
}

void curlGlobalCleanup() {
     curl_global_cleanup();
}