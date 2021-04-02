/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2020, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/ 
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
/* somewhat unix-specific */ 
#include <sys/time.h>
#include <unistd.h>
 
/* curl stuff */ 
#include <curl/curl.h>

size_t
curlDataCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
  ) 
  {

    fprintf(stdout, "\ncurlDataCallback() ->");

    char * user_parameter = (char *)userp;  // cast user parameter to correct datatype
    fprintf(stdout, "\n   User Parameter: %s", user_parameter);

    /* DEBUG - print out response */
    fprintf(stdout, "\n   Data Returned:");
    fprintf(stdout,"\n       Size: %d", (int)nmemb);
    fprintf(stdout,"\n       Data: %s", (char*)contents);
    fprintf(stdout, "\n");

    return nmemb; 
  }

int main(void)
{
  CURL * curl_handle;

  curl_global_init(CURL_GLOBAL_ALL);

  curl_handle = curl_easy_init();

  char * user_parameter = "This is a test of the curl callback function user parameter!";
  
  curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "root:impinj");
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curlDataCallback);
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)user_parameter);
  curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
  curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
  curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYSTATUS, 0);
  curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0);

  curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(curl_handle, CURLOPT_URL, "https://192.168.1.14/api/v1/status");

  CURLcode res;

  res = curl_easy_perform(curl_handle);

  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
      curl_easy_strerror(res));
  }


  return 0;
}

