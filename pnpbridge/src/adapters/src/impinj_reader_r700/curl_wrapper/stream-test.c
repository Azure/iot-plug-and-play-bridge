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

// typedef struct curlPollStatic_ARGS {
//   int *bool_continue_ptr;
//   int *quit_loop_ptr;
//   char testarg[100];
//   CURL *static_handle;  
  
// } curlPollStatic_ARGS;

// void *curlPollStatic(void *params)
//  {
//     // make local copies of input variables
//     int *bool_continue = ((struct curlPollStatic_ARGS*)params)->bool_continue_ptr;
//     int *quit = ((struct curlPollStatic_ARGS*)params)->quit_loop_ptr;
//     CURL_Static_Session_Data * session_data = (CURL_Static_Session_Data*)(((struct curlPollStatic_ARGS*)params)->static_handle);

//     fprintf(stdout, "\n**curlPollStatic fn started!**  Test Arg: ");
//     fprintf(stdout, "%s", (char*)((struct curlPollStatic_ARGS*)params)->testarg);
//     fprintf(stdout, ", quit=%d", *quit);
//     fprintf(stdout, ", bool_continue=%d", *bool_continue);

//     int key;
//     int i = 0;
//     while (*quit==0) {
//       usleep(5000000);
//       i++;      
      
//       fprintf(stdout, "\n");
//       fprintf(stdout, "curlPollStatic iteration: %d", i);
//       fprintf(stdout, ", bool_continue: %d", *bool_continue);
//       fprintf(stdout, ", quit: %d", *quit);
//       fprintf(stdout, "\n");

//       // char ** status = curlStaticGet(session_data, "/status");
//       // fprintf(stdout, "\n  Status GET Output: %s", *status);

//     }
//     *bool_continue=0;
//     fprintf(stdout, "\n**Exiting curlPollStatic fn**\n");
//  }

int main(void)
{
  int still_running = 1; /* keep number of running handles */ 
  int bool_continue = 1;
  int quit_loop = 0;
 
 
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL * curl_handle = curl_easy_init();

  char * http_username = "root";
  char * http_password = "impinj";
  char * http_basepath = "https://192.168.1.14/api/v1";

  char fullStreamUrl[1000] = "";

  strcat(fullStreamUrl, http_basepath);
  strcat(fullStreamUrl, "/data/stream");

  char* full_stream_endpoint = Str_Trim(fullStreamUrl).strPtr;

  fprintf(stdout, " STREAM Endpoint: %s\n", full_stream_endpoint);

  struct CURL_Stream_Session_Data *session_data;
  session_data = curlStreamInit(http_username, http_password, http_basepath, VERIFY_CERTS_OFF, VERBOSE_OUTPUT_OFF);

  // curl_easy_setopt(curl_handle, CURLOPT_USERPWD, "root:impinj");
  // curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curlDummyCallback);
  // curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)session_data);
  // curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, VERIFY_CERTS_OFF);
  // curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, VERIFY_CERTS_OFF);
  // curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYSTATUS, VERIFY_CERTS_OFF);
  // curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, VERBOSE_OUTPUT_ON);
  curl_easy_setopt(session_data->curlHandle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(session_data->curlHandle, CURLOPT_URL, full_stream_endpoint);
 
  CURLM * multi_handle = curl_multi_init();

  curl_multi_add_handle(multi_handle, session_data->curlHandle);

  // struct curlPollStatic_ARGS *curlPollStatic_args_ptr, curlPollStatic_args;
  // curlPollStatic_args.bool_continue_ptr=&bool_continue;
  // curlPollStatic_args.quit_loop_ptr=&quit_loop;
  // strcpy(curlPollStatic_args.testarg,"test text!");
  // curlPollStatic_args.static_handle = static_handle;

  // curlPollStatic_args_ptr=&curlPollStatic_args;

  // pthread_t tid;
  // int rc;
  // fprintf(stdout, "\nSpawning thread for key polling...\n");
  // rc = pthread_create(&tid, NULL, curlPollStatic, (void *)curlPollStatic_args_ptr);

  CURLMcode mc; /* curl_multi_poll() return code */ 
  int numfds = 0;
  int count = 0;
  while(still_running & bool_continue) {
    
    /* we start some action by calling perform right away */  
    mc = curl_multi_perform(multi_handle, &still_running);

    if(still_running & mc == CURLM_OK)
      /* wait for activity, timeout or "nothing" */ 
      mc = curl_multi_poll(multi_handle, NULL, 0, 100, &numfds);
      // fprintf(stdout, "\n Multipoll Thread: Iteration %d", count);
      // count++;
 
    if(mc != CURLM_OK) {
      fprintf(stderr, "curl_multi_wait() failed, code %d.\n", mc);
      break;
    }
  }
 
  // rc = pthread_join(tid, NULL);

  curl_multi_remove_handle(multi_handle, curl_handle);
  curl_multi_cleanup(multi_handle);
  curl_global_cleanup();
 
  return 0;
}