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
// #include <curl/curl.h>

/* helpers */

int main(void)
{
 
  curlGlobalInit();

  char * http_username = "root";
  char * http_password = "impinj";
  char * http_basepath = "https://192.168.1.14/api/v1";
  int httpStatus;

  CURL_Static_Session_Data *static_session;
  static_session = curlStaticInit(http_username, http_password, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  CURL_Stream_Session_Data *stream_session;
  stream_session = curlStreamInit(http_username, http_password, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  char * response;
  response = curlStaticPost(static_session, "/profiles/inventory/presets/default/start", "", &httpStatus);  // start basic_inventory preset
  fprintf(stdout, " \nHTTP Status: %d\n curlStaticPost() Response: %s", httpStatus, response);

  curlStreamSpawnReaderThread(stream_session);

  clock_t mSecInit = clock();

  clock_t mSecTarget = 50000;

  clock_t mSecTimer = 0;

  fprintf(stdout, "\nCLOCKS_PER_SEC: %d", (int)CLOCKS_PER_SEC);

  while (mSecTimer < mSecTarget) {
    
    fprintf(stdout, "\n*************** Wait Thread( Timer: %d uSec, Target: %d uSec): Reading Stream Data Out... **********************************", (int)mSecTimer, (int)mSecTarget);

    int remainingData = 1;
    while (remainingData > 0) // read all data out of buffer, exit on empty buffer
      {
        CURL_Stream_Read_Data read_data = curlStreamReadBufferChunk(stream_session);
        remainingData = read_data.remainingData;

        fprintf(stdout, "\n  STREAM READ( DATA SIZE: %d, DATA REMAINING %d): %s", read_data.dataChunkSize, read_data.remainingData, read_data.dataChunk);
      }

    usleep(1000000);

    mSecTimer = clock() - mSecInit;
  }

  response = curlStaticPost(static_session, "/profiles/stop", "", &httpStatus);  // stop preset
  fprintf(stdout, " \nHTTP Status: %d\n curlStaticPost() Response: %s", httpStatus, response);

  curlStreamStopThread(stream_session);

  curlStreamCleanup(stream_session);
  
  curlGlobalCleanup();
   
  return 0;
}