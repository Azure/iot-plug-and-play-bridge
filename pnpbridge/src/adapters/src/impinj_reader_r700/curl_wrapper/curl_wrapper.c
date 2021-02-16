#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../helpers/string_manipulation.h"
#include <stdint.h>
#include <pthread.h>

void 
curlGlobalInit() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t 
curlStaticDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
  ) 
  {

    size_t realsize = size * nmemb;
    
    struct CURL_Static_Session_Data *session_data = (struct CURL_Static_Session_Data*)userp;

    memcpy(session_data->readCallbackData, contents, nmemb);
    session_data->readCallbackDataLength = (int)nmemb;

    /* DEBUG - print out response */
    // fprintf(stdout,"\n   Size: %d", session_data->readCallbackDataLength);
    // fprintf(stdout,"\n   %s", (char*)*(session_data->readCallbackData));
    // fprintf(stdout, "\n");

  return nmemb; 
}

CURL_Stream_Read_Data 
curlStreamReadBufferChunk(
  CURL_Stream_Session_Data *session_data
)  {
      CURL_Stream_Read_Data read_data;

      if (session_data->bufferReadIndex == session_data->bufferWriteIndex) {
        read_data.dataChunk=NULL;
        read_data.dataChunkSize=0;
        read_data.remainingData=session_data->bufferWriteCounter - session_data->bufferReadCounter;
        return read_data;
      }
      
      char * dataChunk = session_data->dataBuffer + session_data->bufferReadIndex;   // read first chunk of data from buffer

      if ((strlen(dataChunk) + session_data->bufferReadIndex) >= (session_data->dataBufferSize-1)) {  // if the data chunk wraps around end of buffer space, need to reassemble
            
            int overrun =  strlen(dataChunk) + session_data->bufferReadIndex - (session_data->dataBufferSize);
            int firstChunkLength = strlen(dataChunk) - overrun;
            char * firstDataChunk = (char*)malloc(sizeof(char)*strlen(dataChunk)+1);
            memcpy(firstDataChunk, dataChunk, firstChunkLength);

            char * secondDataChunk = session_data->dataBuffer;
            int fullJoinedLength = strlen(firstDataChunk) + strlen(secondDataChunk);
            char * joinedDataChunk = (char*)malloc(sizeof(char)*fullJoinedLength);
            strcat(joinedDataChunk, firstDataChunk);
            strcat(joinedDataChunk, secondDataChunk);

            session_data->bufferReadIndex = strlen(secondDataChunk)+1;


            char * joinedDataChunkReturn = Str_Trim(joinedDataChunk).strPtr;

            session_data->bufferReadCounter += firstChunkLength + strlen(secondDataChunk) + 1;

            if (session_data->bufferReadCounter % session_data->dataBufferSize != session_data->bufferReadIndex) {
                  read_data.dataChunk=NULL;
                  read_data.dataChunkSize=0;
                  read_data.remainingData=session_data->bufferWriteCounter - session_data->bufferReadCounter;
                  
                  fprintf(stdout, "\n  ERROR: Stream data buffer read index mismatch.");
                  
                  return read_data;
            }

            // fprintf(stdout, "\n JOINED DATA: %s", joinedDataChunkReturn);
            // fprintf(stdout, "\n   POST READ - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);

            free(firstDataChunk);
            free(joinedDataChunk);
            
            read_data.dataChunk = joinedDataChunkReturn;
            read_data.dataChunkSize = strlen(joinedDataChunkReturn);
            read_data.remainingData=session_data->bufferWriteCounter - session_data->bufferReadCounter;

            return read_data;
      }

      else {  // if the chunk doesn't wrap around end of memory space, it should all be contained in first read
              session_data->bufferReadIndex += strlen(dataChunk) + 1;
              session_data->bufferReadCounter += strlen(dataChunk) + 1;

              // fprintf(stdout, "\n   POST READ - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);
              read_data.dataChunk = dataChunk;
              read_data.dataChunkSize = strlen(dataChunk);
              read_data.remainingData=session_data->bufferWriteCounter - session_data->bufferReadCounter;
              
              return read_data;
      }
}

size_t 
curlDummyCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
 ) {

   struct CURL_Stream_Session_Data *session_data = (struct CURL_Stream_Session_Data*)userp;

   fprintf(stdout, "\nRESPONSE: %s", (char*)contents);
   return nmemb;
 }

void 
curlStreamBufferReadout(
  CURL_Stream_Session_Data *session_data
  ) 
  {
    
    fprintf(stdout, "\n BUFFER DATA: ");
    for (int i = 0; i < session_data->dataBufferSize; i++)
    fprintf(stdout, "%c", *(session_data->dataBuffer + i));
    fprintf(stdout, "\n");

  }

size_t 
curlStreamDataReadCallback(
  void *contents, 
  size_t size, 
  size_t nmemb, 
  void *userp
 ) {
    
    size_t realsize = size * nmemb;
    
    struct CURL_Stream_Session_Data *session_data = (struct CURL_Stream_Session_Data*)userp;  // cast input pointer to session data

    if (((session_data->bufferWriteIndex + 1) % session_data->dataBufferSize) == session_data->dataBufferSize) {
      fprintf(stdout, "  ERROR: Stream data buffer overflow.");
      return -1;
    }

    // memset(session_data->dataBuffer, 'X', session_data->dataBufferSize); // DEBUG

    int spaceUntilWrap = session_data->dataBufferSize - session_data->bufferWriteIndex;
    
    // fprintf(stdout, "\n       PRE WRITE - BufferWriteIndex: %d, BufferReadIndex: %d, SpaceUntilWrap: %d, BufferSize: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex, spaceUntilWrap, session_data->dataBufferSize);

    if (nmemb > spaceUntilWrap) {  // wrap around end of buffer
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, spaceUntilWrap);
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, spaceUntilWrap);
        memcpy(session_data->dataBuffer, (char*)contents + spaceUntilWrap, nmemb - spaceUntilWrap);
        session_data->bufferWriteIndex = nmemb - spaceUntilWrap;
    }
    else {
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, nmemb);
        session_data->bufferWriteIndex = session_data->bufferWriteIndex + (int)nmemb;
    }

    // follow contents with null char
    memset(session_data->dataBuffer + session_data->bufferWriteIndex, '\000', 1);
    session_data->bufferWriteIndex += 1;

    session_data->bufferWriteCounter += nmemb + 1;

    int64_t bufferAllocation = session_data->bufferWriteCounter - session_data->bufferReadCounter;

    int checkWriteCounter = session_data->bufferWriteCounter % session_data->dataBufferSize;
    // fprintf(stdout, "\n WRITECOUNTCALC: %d, WRITEINDEX: %d", checkWriteCounter, session_data->bufferWriteIndex); // debug

    if (checkWriteCounter != session_data->bufferWriteIndex)
     {
        fprintf(stdout, "\n  ERROR: Stream data buffer write index mismatch.");
        return -1;
     }    

     if (bufferAllocation >= session_data->dataBufferSize) {
        fprintf(stdout, "\n  ERROR: Stream data buffer overflow (writes faster than reads).");
        return -1;
     }

    spaceUntilWrap = session_data->dataBufferSize - session_data->bufferWriteIndex;

    // fprintf(stdout, "\n      POST WRITE - BufferWriteIndex: %d, BufferReadIndex: %d, SpaceUntilWrap: %d, BufferSize: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex, spaceUntilWrap, session_data->dataBufferSize);

    // print out the entire buffer character space
    // curlStreamBufferReadout(session_data); //DEBUG
    
    // DEBUG readout 
    // char * streamDataChunk = curlStreamReadBufferChunk(session_data);
    // fprintf(stdout, "\n  StreamDataLength: %d", (int)strlen(streamDataChunk));
    // fprintf(stdout, "\n  StreamData: %s", streamDataChunk);

  return nmemb;
 }

void *
curlStreamReader(
  void * sessionData
  )
  {
    fprintf(stdout, "\nINFO: cURL stream reader thread started.");

    CURL_Stream_Session_Data * session_data = (CURL_Stream_Session_Data*)sessionData; 
    
    CURLMcode mc; /* curl_multi_poll() return code */ 
    int numfds = 0;
    int count = 0;
    int still_running = 1;
    while(still_running & (session_data->threadData.stopFlag == 0)) {
      
      /* we start some action by calling perform right away */  
      mc = curl_multi_perform(session_data->multiHandle, &still_running);

      if(still_running & mc == CURLM_OK)
        /* wait for activity, timeout or "nothing" */ 
        mc = curl_multi_poll(session_data->multiHandle, NULL, 0, 100, &numfds);
        // fprintf(stdout, "\n Multipoll Thread: Iteration %d", count);
        // count++;
  
      if(mc != CURLM_OK) {
        fprintf(stderr, "\ncurl_multi_wait() failed in curlStreamReader(), code %d.", mc);
        break;
      }
    }
    fprintf(stdout, "\nINFO: cURL stream reader thread shut down.");
  }

int 
curlStreamSpawnReaderThread(
  CURL_Stream_Session_Data * session_data
  )
  {
    session_data->threadData.thread_ref = pthread_create(&(session_data->threadData.tid), NULL, curlStreamReader, (void *)session_data);
  }

int
curlStreamStopThread(
  CURL_Stream_Session_Data * session_data
  )
  {
    session_data->threadData.stopFlag = 1;
  }

CURL_Static_Session_Data * 
curlStaticInit(
  char *username,                        
  char *password, 
  char *basePath, 
  int EnableVerify,  
  long verboseOutput
  ) 
  {
  
  CURL *static_handle;
  static_handle = curl_easy_init();

  #define STATIC_READ_CALLBACK_DATA_BUFFER_SIZE 10000

  static char *read_callback_data;
  read_callback_data = (char *)malloc(sizeof(char)*STATIC_READ_CALLBACK_DATA_BUFFER_SIZE);

  #define STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE 10000

  static char *write_callback_data;
  write_callback_data = (char *)malloc(sizeof(char)*STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE);


  
  if(read_callback_data == NULL) {
    /* out of memory! */ 
    printf("not enough memory for cURL callback data buffer (malloc returned NULL)\n");
    return 0;
  }

  // build user:password string for cURL
  char usrpwd_build[256] = "";
  strcat(usrpwd_build, username); 
  strcat(usrpwd_build, ":"); 
  strcat(usrpwd_build, password);

  char* usrpwd = Str_Trim(usrpwd_build).strPtr;

  // Set session data values
  #define staticInitArraySize 10
  int structInitSizes[staticInitArraySize] = {
      sizeof(CURL*),  //curl session pointer
      sizeof(int*),   //username pointer
      sizeof(int),    //username size
      sizeof(int*),   //password pointer
      sizeof(int),    //password size
      sizeof(int*),   //basepath pointer
      sizeof(int),    //basepath size
      sizeof(char**), //readCallbackData pointer
      sizeof(int),    //readCallbackData size
      sizeof(int)     //callbackBuffer Size
  };

  CURL_Static_Session_Data *session_data = malloc(sizeof(CURL_Static_Session_Data));
  
    session_data->curlHandle = static_handle;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);
    session_data->readCallbackData = read_callback_data;
    session_data->readCallbackDataLength = 0;
    session_data->readCallbackBufferSize = STATIC_READ_CALLBACK_DATA_BUFFER_SIZE;
    session_data->writeCallbackData = write_callback_data;
    session_data->writeCallbackDataLength = 0;
    session_data->writeCallbackBufferSize = STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE;

// apply curl settings with libcurl calls
  curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, curlStaticDataReadCallback);
  curl_easy_setopt(static_handle, CURLOPT_WRITEDATA, (void *)session_data);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(static_handle, CURLOPT_VERBOSE, verboseOutput);

// test curl configuration
  char* response;

  response = curlStaticGet(session_data, "/status");
      
  fprintf(stdout, "\nInitialization test Call: %s\n", response);

  return session_data;

}

CURL_Stream_Session_Data * curlStreamInit(
  char *username,                        
  char *password, 
  char *basePath, 
  int EnableVerify,  
  long verboseOutput
  ) 
  {
  
  CURL *stream_handle;
  stream_handle = curl_easy_init();

  #define STREAM_DATA_BUFFER_SIZE 100000
  char * streamBuffer;
  streamBuffer = (char*)malloc(sizeof(char)*STREAM_DATA_BUFFER_SIZE);

  memset(streamBuffer, '\0', STREAM_DATA_BUFFER_SIZE);
  
  if(streamBuffer == NULL) {
    /* out of memory! */ 
    printf("not enough memory for cURL callback data buffer (malloc returned NULL)\n");
    return 0;
  }

  // build user:password string for cURL
  char usrpwd_build[256] = "";
  strcat(usrpwd_build, username); 
  strcat(usrpwd_build, ":"); 
  strcat(usrpwd_build, password);

  char* usrpwd = Str_Trim(usrpwd_build).strPtr;

    
    // initialize session data structure
    CURL_Stream_Session_Data *session_data = malloc(sizeof(CURL_Stream_Session_Data));
  
    session_data->curlHandle = stream_handle;
    session_data->threadData.stopFlag = 0;
    session_data->username = username;
    session_data->usernameLength = strlen(username);
    session_data->password = password;
    session_data->passwordLength = strlen(password);
    session_data->basePath = basePath;
    session_data->basePathLength = strlen(basePath);
    session_data->dataBuffer = streamBuffer;
    session_data->dataBufferSize = STREAM_DATA_BUFFER_SIZE;
    session_data->bufferReadIndex = 0;
    session_data->bufferReadCounter = 0;
    session_data->bufferWriteIndex = 0;
    session_data->bufferWriteCounter = 0;

// apply curl settings with libcurl calls
  curl_easy_setopt(stream_handle, CURLOPT_USERPWD, usrpwd);
  curl_easy_setopt(stream_handle, CURLOPT_WRITEFUNCTION, curlStreamDataReadCallback);
  curl_easy_setopt(stream_handle, CURLOPT_WRITEDATA, (void *)session_data);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
  curl_easy_setopt(stream_handle, CURLOPT_VERBOSE, verboseOutput);

  char fullStreamUrl[1000] = "";
  
  strcat(fullStreamUrl, session_data->basePath);
  strcat(fullStreamUrl, "/data/stream");

  char* full_stream_endpoint = Str_Trim(fullStreamUrl).strPtr;

  curl_easy_setopt(session_data->curlHandle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(session_data->curlHandle, CURLOPT_URL, full_stream_endpoint);
  fprintf(stdout, " STREAM Endpoint: %s\n", full_stream_endpoint);

// initialize multi handle
  session_data->multiHandle = curl_multi_init();
  curl_multi_add_handle(session_data->multiHandle, session_data->curlHandle);

  return session_data;
}


char*
curlStaticGet(
  CURL_Static_Session_Data *session_data, 
  char *endpoint
  ) 
  {
  
  CURL *static_handle = session_data->curlHandle;

  char* startPtr = session_data->readCallbackData;

  memset(startPtr, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));  //re-initialize memory space

  session_data->readCallbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  // fprintf(stdout, " GET Endpoint: %s\n", full_endpoint); //DEBUG

  curl_easy_setopt(static_handle, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);

  CURLcode res;

  res = curl_easy_perform(static_handle);

  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed in curlStaticGet: %s\n",
      curl_easy_strerror(res));
  }

  return session_data->readCallbackData;
}

char*
curlStaticPost(
  CURL_Static_Session_Data *session_data, 
  char *endpoint, 
  char *postData
  ) {
  
  CURL *static_handle = session_data->curlHandle;

  // re-initialize callback data buffer
  char* startPtr = session_data->readCallbackData;
  memset(startPtr, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
  session_data->readCallbackDataLength = 0;

  char fullurl[1000] = "";

  strcat(fullurl, session_data->basePath);
  strcat(fullurl, endpoint);

  char* full_endpoint = Str_Trim(fullurl).strPtr;

  fprintf(stdout, " POST Endpoint: %s\n", full_endpoint);

  curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
  curl_easy_setopt(static_handle, CURLOPT_POST, 1);
  curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, postData);

  CURLcode res;

  res = curl_easy_perform(static_handle);

  if(res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed in curlStaticPost: %s\n",
        curl_easy_strerror(res));
  }

  return session_data->readCallbackData;
}

size_t 
curlStaticDataWriteCallback(
  void *write_data, 
  size_t size, 
  size_t nmemb, 
  void *userp
 ) {
    CURL_Static_Session_Data * session_data = (CURL_Static_Session_Data*)userp;

    int offset = strlen(session_data->writeCallbackData) - session_data->writeCallbackDataLength;  // using writeCallbackDataLength as remaining data to write

    int curlWriteBufferSize = size * nmemb;

    // fprintf(stdout, "     PUT cURL buffer size (bytes): %d\n", curlWriteBufferSize);
    // fprintf(stdout, "        PUT cURL size: %d\n", (int)size);
    // fprintf(stdout, "        PUT cURL nmemb: %d\n", (int)nmemb);
    // fprintf(stdout, "     PUT data size (bytes): %d\n", session_data->writeCallbackDataLength);
    // fprintf(stdout, "     PUT data to write: %s\n", session_data->writeCallbackData + offset);

    if (session_data->writeCallbackDataLength * sizeof(char) >= curlWriteBufferSize) {  // data to write is larger than cURL buffer, write first chunk
      
      // fprintf(stdout, "        Data offset: %d\n", offset);

      memcpy(write_data, (session_data->writeCallbackData) + offset, curlWriteBufferSize);
      session_data->writeCallbackDataLength -= curlWriteBufferSize;  // update remaining data to write
      return curlWriteBufferSize;
    }

    else {  // data to write will fit in cURL buffer, write everything
      memcpy(write_data, session_data->writeCallbackData + offset, strlen(session_data->writeCallbackData)*sizeof(char)+1);

      size_t handled_bytes = strlen(session_data->writeCallbackData)*sizeof(char);

      memset(session_data->writeCallbackData, '\0', STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
      session_data->writeCallbackDataLength = 0;  // no more data to write

      return handled_bytes;  // must return number of bytes handled.  Return 0 to terminate.
    }
 }

char* 
curlStaticPut(
  CURL_Static_Session_Data *session_data, 
  char *endpoint, 
  char *putData
  ) {

    CURL *static_handle = session_data->curlHandle;

    // re-initialize callback data buffer
    memset(session_data->readCallbackData, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->readCallbackDataLength = 0;

    memset(session_data->writeCallbackData, '\0', STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->writeCallbackDataLength = 0;

    memcpy(session_data->writeCallbackData, putData, strlen(putData)*sizeof(char));
    session_data->writeCallbackDataLength = strlen(putData);

    char fullurl[1000] = "";

    strcat(fullurl, session_data->basePath);
    strcat(fullurl, endpoint);

    char* full_endpoint = Str_Trim(fullurl).strPtr;

    fprintf(stdout, " PUT Endpoint: %s\n", full_endpoint);

    curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
    curl_easy_setopt(static_handle, CURLOPT_READFUNCTION, curlStaticDataWriteCallback);
    curl_easy_setopt(static_handle, CURLOPT_READDATA, (void *)session_data);
    curl_easy_setopt(static_handle, CURLOPT_UPLOAD, 1L);
    // curl_easy_setopt(static_handle, CURLOPT_INFILESIZE, (strlen(putData)) * sizeof(char));  // only needed for HTTP 1.x
 
    CURLcode res = curl_easy_perform(static_handle);

    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed in curlStaticPost: %s\n",
          curl_easy_strerror(res));
    }

    return session_data->readCallbackData; // TODO: implement error handling
  }

void 
curlStaticCleanup(
  CURL_Static_Session_Data *session_data
  ) {
    curl_easy_cleanup(session_data->curlHandle);
    free(session_data->readCallbackData);
    free(session_data->writeCallbackData);
    free(session_data);
  }

void 
curlStreamCleanup(
  CURL_Stream_Session_Data *session_data
  ) {
    session_data->threadData.thread_ref = pthread_join(session_data->threadData.tid, NULL);
    curl_multi_remove_handle(session_data->multiHandle, session_data->curlHandle);
    curl_multi_cleanup(session_data->multiHandle);
    curl_easy_cleanup(session_data->curlHandle);
    free(session_data->dataBuffer);
    free(session_data);
  }


void 
curlGlobalCleanup() {
     curl_global_cleanup();
}

