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
#include "curl_wrapper.h"
// #include <curl/curl.h>

/* helpers */
#include "../helpers/string_manipulation.h"

int main(void)
{
 
  curlGlobalInit();

  char * http_username = "root";
  char * http_password = "impinj";
  char * http_basepath = "https://192.168.1.14/api/v1";

  struct CURL_Stream_Session_Data *session_data;
  session_data = curlStreamInit(http_username, http_password, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  curlStreamSpawnThread(session_data);

  for (int i=0; i<10; i++){
    fprintf(stdout, "\nWait Thread: %d", i);
    usleep(1000000);
  }

  curlStreamStopThread(session_data);

  curlStreamCleanup(session_data);
  
  curlGlobalCleanup();
   
  return 0;
}