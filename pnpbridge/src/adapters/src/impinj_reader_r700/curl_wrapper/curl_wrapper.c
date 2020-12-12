#include "curl_wrapper.h"

size_t DataReadCallback(void *contents, size_t size, size_t nmemb, void *userp) {

    fprintf(stdout,"\n   Size: %d", (int)nmemb);
    fprintf(stdout,"\n   %s", (char *)contents);
 
  return nmemb; // realsize;
}