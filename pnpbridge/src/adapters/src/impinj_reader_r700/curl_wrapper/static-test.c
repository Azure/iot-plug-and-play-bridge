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

#define USEC_DELAY 1

int main(void)
{
  CURL_Session_Data *static_session;

  char** res;

  size_t (*callbackFunction)() = &curlStaticDataReadCallback;

  curlGlobalInit();
 
  static_session = curlStaticInit("root", "impinj", "https://192.168.1.14/api/v1", VERIFY_CERTS_OFF, callbackFunction, VERBOSE_OUTPUT_OFF);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/profiles/inventory/presets");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  res = curlStaticPost(static_session, "/profiles/inventory/presets/basic_inventory/start", "");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  res = curlStaticPost(static_session, "/profiles/stop", "");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  res = curlStaticGet(static_session, "/status");
  fprintf(stdout, "    Response: %s\n", *res);

  usleep(USEC_DELAY);

  curlStaticCleanup(static_session);

  curlGlobalCleanup();
 
  return 0;
}

