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
/* <DESC>
 * single download with the multi interface's curl_multi_poll
 * </DESC>
 */ 
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
 
/* somewhat unix-specific */ 
#include <sys/time.h>
#include <unistd.h>
 
/* curl stuff */ 
#include "../curl_wrapper.h"

#define USEC_DELAY 1
#define DEBUG_CURL

int main(void)
{
  char * res;
  int httpStatus;

  PURL_DATA urlData = (PURL_DATA)malloc(sizeof(URL_DATA)); 

  const char* constUrl = "https://192.168.1.14/api/v1/system/rfid/interface";
  const char* constUsername = "root";
  const char* constPassword = "impinj";

  strcpy(urlData->url, constUrl);
  strcpy(urlData->username, constUsername);
  strcpy(urlData->password, constPassword);

  char buffer[100] = {0};

  httpStatus = curlGet(urlData, buffer);
  fprintf(stdout, "    curlGet(%s): %d\n    Response: %s\n", urlData->url, httpStatus, buffer);

  free(urlData);

  curlGlobalInit();
 
  CURL_Static_Session_Data *static_session = curlStaticInit(constUsername, constPassword, "https://192.168.1.14/api/v1", Session_Static, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/profiles/inventory/presets", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  char * presetJson = malloc(sizeof(char)*10000);

  res = curlStaticGet(static_session, "/profiles/inventory/presets/default", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  strcpy(presetJson, res);

  res = curlStaticPut(static_session, "/profiles/inventory/presets/test", presetJson, &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/profiles/inventory/presets", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticPut(static_session, "/http-stream", "{\"eventBufferSize\": 0,\"eventPerSecondLimit\": 0,\"eventAgeLimitMinutes\": 0}", &httpStatus); // this should result in an error response from reader
  fprintf(stdout, "    HTTP Status: %d\n    Response (expected error): %s\n", httpStatus, res);  

  usleep(USEC_DELAY);

  res = curlStaticPut(static_session, "/http-stream", "{\"eventBufferSize\": 0,\"eventPerSecondLimit\": 0,\"eventAgeLimitMinutes\": 10}", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/http-stream", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticPost(static_session, "/profiles/inventory/presets/garbage_broken_path/start", "", &httpStatus);   // this should result in an error response from reader
  fprintf(stdout, "    HTTP Status: %d\n    Response (expected error): %s\n", httpStatus, res);  

  usleep(USEC_DELAY);

  res = curlStaticPost(static_session, "/profiles/inventory/presets/test/start", "", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticPost(static_session, "/profiles/stop", "", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticDelete(static_session, "/profiles/inventory/presets/test", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/profiles/inventory/presets", &httpStatus);
  fprintf(stdout, "    HTTP Status: %d\n    Response: %s\n", httpStatus, res);

  usleep(USEC_DELAY);

  curlStaticCleanup(static_session);

  curlGlobalCleanup();

  free(presetJson);
 
  return 0;
}

