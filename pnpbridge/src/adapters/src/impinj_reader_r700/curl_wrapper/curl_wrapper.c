#include "curl_wrapper.h"
#include "curl/curl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <assert.h>
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"

//#define DEBUG_CURL

void curlGlobalInit()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

size_t
curlStaticDataReadCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{

    size_t realsize = size * nmemb;

    struct CURL_Static_Session_Data* session_data = (struct CURL_Static_Session_Data*)userp;

    memcpy(session_data->readCallbackData, contents, nmemb);
    session_data->readCallbackDataLength = (int)nmemb;

    /* DEBUG - print out response */
    // fprintf(stdout,"\n   Size: %d", session_data->readCallbackDataLength);
    // fprintf(stdout,"\n   %s", (char*)*(session_data->readCallbackData));
    // LogInfo("R700 :");

    return nmemb;
}

CURL_Stream_Read_Data
curlStreamReadBufferChunk(
    CURL_Stream_Session_Data* session_data)
{
    CURL_Stream_Read_Data read_data;

    if (session_data->bufferReadIndex == session_data->bufferWriteIndex)
    {
        read_data.dataChunk     = NULL;
        read_data.dataChunkSize = 0;
        read_data.remainingData = session_data->bufferWriteCounter - session_data->bufferReadCounter;
        return read_data;
    }

    char* dataChunk = session_data->dataBuffer + session_data->bufferReadIndex;   // read first chunk of data from buffer

    if ((strlen(dataChunk) + session_data->bufferReadIndex) >= (session_data->dataBufferSize - 1))
    {   // if the data chunk wraps around end of buffer space, need to reassemble

        int overrun          = strlen(dataChunk) + session_data->bufferReadIndex - (session_data->dataBufferSize);
        int firstChunkLength = strlen(dataChunk) - overrun;
        char* firstDataChunk = (char*)malloc(sizeof(char) * strlen(dataChunk) + 1);
        memcpy(firstDataChunk, dataChunk, firstChunkLength);

        char* secondDataChunk = session_data->dataBuffer;
        int fullJoinedLength  = strlen(firstDataChunk) + strlen(secondDataChunk);
        char* joinedDataChunk = (char*)malloc(sizeof(char) * fullJoinedLength);
        strcat(joinedDataChunk, firstDataChunk);
        strcat(joinedDataChunk, secondDataChunk);

        session_data->bufferReadIndex = strlen(secondDataChunk) + 1;

        char* joinedDataChunkReturn = NULL;

        mallocAndStrcpy_s(&joinedDataChunkReturn, joinedDataChunk);

        session_data->bufferReadCounter += firstChunkLength + strlen(secondDataChunk) + 1;

        if (session_data->bufferReadCounter % session_data->dataBufferSize != session_data->bufferReadIndex)
        {
            read_data.dataChunk     = NULL;
            read_data.dataChunkSize = 0;
            read_data.remainingData = session_data->bufferWriteCounter - session_data->bufferReadCounter;

            LogInfo("R700 :\n  ERROR: Stream data buffer read index mismatch.");

            return read_data;
        }

        // LogInfo("R700 :\n JOINED DATA: %s", joinedDataChunkReturn);
        // LogInfo("R700 :\n   POST READ - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);

        free(firstDataChunk);
        free(joinedDataChunk);

        read_data.dataChunk     = joinedDataChunkReturn;
        read_data.dataChunkSize = strlen(joinedDataChunkReturn);
        read_data.remainingData = session_data->bufferWriteCounter - session_data->bufferReadCounter;

        return read_data;
    }

    else
    {   // if the chunk doesn't wrap around end of memory space, it should all be contained in first read
        session_data->bufferReadIndex += strlen(dataChunk) + 1;
        session_data->bufferReadCounter += strlen(dataChunk) + 1;

        // LogInfo("R700 :\n   POST READ - BufferWriteIndex: %d, BufferReadIndex: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex);
        read_data.dataChunk     = dataChunk;
        read_data.dataChunkSize = strlen(dataChunk);
        read_data.remainingData = session_data->bufferWriteCounter - session_data->bufferReadCounter;

        return read_data;
    }
}

void curlStreamBufferReadout(
    CURL_Stream_Session_Data* session_data)
{

    LogInfo("R700 :\n BUFFER DATA: ");
    for (int i = 0; i < session_data->dataBufferSize; i++)
        LogInfo("R700 :%c", *(session_data->dataBuffer + i));
    LogInfo("R700 :");
}

size_t
curlStreamDataReadCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    size_t realsize = size * nmemb;

    struct CURL_Stream_Session_Data* session_data = (struct CURL_Stream_Session_Data*)userp;   // cast input pointer to session data

    if (((session_data->bufferWriteIndex + 1) % session_data->dataBufferSize) == session_data->dataBufferSize)
    {
        LogError("R700 : Stream data buffer overflow.");
        return -1;
    }

    // memset(session_data->dataBuffer, 'X', session_data->dataBufferSize); // DEBUG

    int spaceUntilWrap = session_data->dataBufferSize - session_data->bufferWriteIndex;

    // LogInfo("R700 :\n       PRE WRITE - BufferWriteIndex: %d, BufferReadIndex: %d, SpaceUntilWrap: %d, BufferSize: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex, spaceUntilWrap, session_data->dataBufferSize);

    if (nmemb > spaceUntilWrap)
    {   // wrap around end of buffer
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, spaceUntilWrap);
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, spaceUntilWrap);
        memcpy(session_data->dataBuffer, (char*)contents + spaceUntilWrap, nmemb - spaceUntilWrap);
        session_data->bufferWriteIndex = nmemb - spaceUntilWrap;
    }
    else
    {
        memcpy(session_data->dataBuffer + session_data->bufferWriteIndex, (char*)contents, nmemb);
        session_data->bufferWriteIndex = session_data->bufferWriteIndex + (int)nmemb;
    }

    // follow contents with null char
    memset(session_data->dataBuffer + session_data->bufferWriteIndex, '\000', 1);
    session_data->bufferWriteIndex += 1;

    session_data->bufferWriteCounter += nmemb + 1;

    int64_t bufferAllocation = session_data->bufferWriteCounter - session_data->bufferReadCounter;

    int checkWriteCounter = session_data->bufferWriteCounter % session_data->dataBufferSize;
    // LogInfo("R700 :\n WRITECOUNTCALC: %d, WRITEINDEX: %d", checkWriteCounter, session_data->bufferWriteIndex); // debug

    if (checkWriteCounter != session_data->bufferWriteIndex)
    {
        LogError("R700 :\nStream data buffer write index mismatch.");
        return -1;
    }

    if (bufferAllocation >= session_data->dataBufferSize)
    {
        LogError("R700 :\nERROR: Stream data buffer overflow (writes faster than reads).");
        return -1;
    }

    spaceUntilWrap = session_data->dataBufferSize - session_data->bufferWriteIndex;

    // LogInfo("R700 :\n      POST WRITE - BufferWriteIndex: %d, BufferReadIndex: %d, SpaceUntilWrap: %d, BufferSize: %d", session_data->bufferWriteIndex, session_data->bufferReadIndex, spaceUntilWrap, session_data->dataBufferSize);

    // print out the entire buffer character space
    // curlStreamBufferReadout(session_data); //DEBUG

    // DEBUG readout
    // char * streamDataChunk = curlStreamReadBufferChunk(session_data);
    // LogInfo("R700 :\n  StreamDataLength: %d", (int)strlen(streamDataChunk));
    // LogInfo("R700 :\n  StreamData: %s", streamDataChunk);

    return nmemb;
}

void*
curlStreamReader(
    void* sessionData)
{
    LogInfo("R700 : %s() enter : cURL stream reader thread started.", __FUNCTION__);

    CURL_Stream_Session_Data* session_data = (CURL_Stream_Session_Data*)sessionData;

    CURLMcode mc; /* curl_multi_poll() return code */
    int numfds = 0;
    //    int count         = 0;
    int still_running = 1;

    while (still_running & (session_data->threadData.stopFlag == 0))
    {
        /* we start some action by calling perform right away */
        mc = curl_multi_perform(session_data->multiHandle, &still_running);

        if (still_running & mc == CURLM_OK)
            /* wait for activity, timeout or "nothing" */
            mc = curl_multi_poll(session_data->multiHandle, NULL, 0, 100, &numfds);

        // LogInfo("R700 :\n Multipoll Thread: Iteration %d", count);
        // count++;

        if (mc != CURLM_OK)
        {
            LogError("R700\ncurl_multi_wait() failed in curlStreamReader(), code %d.", mc);
            break;
        }
    }
    LogInfo("R700 : %s() exit : cURL stream reader thread shut down.", __FUNCTION__);
}

int curlStreamSpawnReaderThread(
    CURL_Stream_Session_Data* session_data)
{
    session_data->threadData.thread_ref = pthread_create(&(session_data->threadData.tid), NULL, curlStreamReader, (void*)session_data);
}

int curlStreamStopThread(
    CURL_Stream_Session_Data* session_data)
{
    session_data->threadData.stopFlag = 1;
}

CURL_Static_Session_Data*
curlStaticInit(
    const char* username,
    const char* password,
    char* basePath,
    int EnableVerify,
    long verboseOutput)
{
    CURL* static_handle;
    static_handle       = curl_easy_init();
    char* username_copy = NULL;
    char* password_copy = NULL;

#define STATIC_READ_CALLBACK_DATA_BUFFER_SIZE 10000

    static char* read_callback_data;
    read_callback_data = (char*)malloc(sizeof(char) * STATIC_READ_CALLBACK_DATA_BUFFER_SIZE);

#define STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE 10000

    static char* write_callback_data;
    write_callback_data = (char*)malloc(sizeof(char) * STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE);

    if (read_callback_data == NULL)
    {
        /* out of memory! */
        LogError("R700 : not enough memory for cURL callback data buffer (malloc returned NULL)");
        return 0;
    }

    // build user:password string for cURL
    char usrpwd_build[256] = "";
    strcat(usrpwd_build, username);
    strcat(usrpwd_build, ":");
    strcat(usrpwd_build, password);

    char* usrpwd = NULL;

    mallocAndStrcpy_s(&usrpwd, usrpwd_build);
    mallocAndStrcpy_s(&username_copy, username);
    mallocAndStrcpy_s(&password_copy, password);

// Set session data values
#define staticInitArraySize 10
    int structInitSizes[staticInitArraySize] = {
        sizeof(CURL*),    //curl session pointer
        sizeof(int*),     //username pointer
        sizeof(int),      //username size
        sizeof(int*),     //password pointer
        sizeof(int),      //password size
        sizeof(int*),     //basepath pointer
        sizeof(int),      //basepath size
        sizeof(char**),   //readCallbackData pointer
        sizeof(int),      //readCallbackData size
        sizeof(int)       //callbackBuffer Size
    };

    CURL_Static_Session_Data* session_data = malloc(sizeof(CURL_Static_Session_Data));

    session_data->curlHandle              = static_handle;
    session_data->username                = username_copy;
    session_data->usernameLength          = strlen(username_copy);
    session_data->password                = password_copy;
    session_data->passwordLength          = strlen(password_copy);
    session_data->basePath                = basePath;
    session_data->basePathLength          = strlen(basePath);
    session_data->readCallbackData        = read_callback_data;
    session_data->readCallbackDataLength  = 0;
    session_data->readCallbackBufferSize  = STATIC_READ_CALLBACK_DATA_BUFFER_SIZE;
    session_data->writeCallbackData       = write_callback_data;
    session_data->writeCallbackDataLength = 0;
    session_data->writeCallbackBufferSize = STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE;

    // apply curl settings with libcurl calls
    curl_easy_setopt(static_handle, CURLOPT_USERPWD, usrpwd);
    curl_easy_setopt(static_handle, CURLOPT_WRITEFUNCTION, curlStaticDataReadCallback);
    curl_easy_setopt(static_handle, CURLOPT_WRITEDATA, (void*)session_data);
    curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
    curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
    curl_easy_setopt(static_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
    curl_easy_setopt(static_handle, CURLOPT_VERBOSE, verboseOutput);

    // test curl configuration
    char* response;

    // response = curlStaticGet(session_data, "/status");

    // LogInfo("R700 :\nInitialization test Call: %s", response);

    if (usrpwd)
    {
        free(usrpwd);
    }

    return session_data;
}

CURL_Stream_Session_Data* curlStreamInit(
    const char* username,
    const char* password,
    char* basePath,
    int EnableVerify,
    long verboseOutput)
{

    CURL* stream_handle;
    stream_handle       = curl_easy_init();
    char* username_copy = NULL;
    char* password_copy = NULL;

#define STREAM_DATA_BUFFER_SIZE 100000

    char* streamBuffer;

    LogInfo("R700 : %s() enter", __FUNCTION__);

    streamBuffer = (char*)malloc(sizeof(char) * STREAM_DATA_BUFFER_SIZE);

    memset(streamBuffer, '\0', STREAM_DATA_BUFFER_SIZE);

    if (streamBuffer == NULL)
    {
        /* out of memory! */
        printf("not enough memory for cURL callback data buffer (malloc returned NULL)");
        return 0;
    }

    // build user:password string for cURL
    char usrpwd_build[256] = "";
    strcat(usrpwd_build, username);
    strcat(usrpwd_build, ":");
    strcat(usrpwd_build, password);

    char* usrpwd = NULL;
    mallocAndStrcpy_s(&usrpwd, usrpwd_build);
    mallocAndStrcpy_s(&username_copy, username);
    mallocAndStrcpy_s(&password_copy, password);

    // initialize session data structure
    CURL_Stream_Session_Data* session_data = malloc(sizeof(CURL_Stream_Session_Data));

    session_data->curlHandle          = stream_handle;
    session_data->threadData.stopFlag = 0;
    session_data->username            = username_copy;
    session_data->usernameLength      = strlen(username_copy);
    session_data->password            = password_copy;
    session_data->passwordLength      = strlen(password_copy);
    session_data->basePath            = basePath;
    session_data->basePathLength      = strlen(basePath);
    session_data->dataBuffer          = streamBuffer;
    session_data->dataBufferSize      = STREAM_DATA_BUFFER_SIZE;
    session_data->bufferReadIndex     = 0;
    session_data->bufferReadCounter   = 0;
    session_data->bufferWriteIndex    = 0;
    session_data->bufferWriteCounter  = 0;

    // apply curl settings with libcurl calls
    curl_easy_setopt(stream_handle, CURLOPT_USERPWD, usrpwd);
    curl_easy_setopt(stream_handle, CURLOPT_WRITEFUNCTION, curlStreamDataReadCallback);
    curl_easy_setopt(stream_handle, CURLOPT_WRITEDATA, (void*)session_data);
    curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYPEER, EnableVerify);
    curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYHOST, EnableVerify);
    curl_easy_setopt(stream_handle, CURLOPT_SSL_VERIFYSTATUS, EnableVerify);
    curl_easy_setopt(stream_handle, CURLOPT_VERBOSE, verboseOutput);

    char fullStreamUrl[1000] = "";

    strcat(fullStreamUrl, session_data->basePath);
    strcat(fullStreamUrl, "/data/stream");

    char* full_stream_endpoint = NULL;

    mallocAndStrcpy_s(&full_stream_endpoint, fullStreamUrl);

    curl_easy_setopt(session_data->curlHandle, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(session_data->curlHandle, CURLOPT_URL, full_stream_endpoint);
    LogInfo("R700 : STREAM Endpoint: %s", full_stream_endpoint);

    // initialize multi handle
    session_data->multiHandle = curl_multi_init();
    curl_multi_add_handle(session_data->multiHandle, session_data->curlHandle);

    if (usrpwd)
    {
        free(usrpwd);
    }

    if (full_stream_endpoint)
    {
        free(full_stream_endpoint);
    }

    return session_data;
}

char* curlStaticGet(
    CURL_Static_Session_Data* session_data,
    char* endpoint,
    int* statusCode)
{
#ifdef DEBUG_CURL
    LogInfo("R700 : %s %s", __FUNCTION__, endpoint);
#endif
    CURL* static_handle = session_data->curlHandle;
    long http_code;

    char* startPtr = session_data->readCallbackData;

    assert(statusCode != NULL);

    memset(startPtr, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));   //re-initialize memory space

    session_data->readCallbackDataLength = 0;

    char fullurl[1000] = "";

    strcat(fullurl, session_data->basePath);
    strcat(fullurl, endpoint);

    char* full_endpoint = NULL;
    mallocAndStrcpy_s(&full_endpoint, fullurl);

    // LogInfo("R700 : GET Endpoint: '%s'", full_endpoint);   //DEBUG

    curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
    curl_easy_setopt(static_handle, CURLOPT_POST, 0L);
    curl_easy_setopt(static_handle, CURLOPT_CUSTOMREQUEST, "GET");

    CURLcode res;

    res = curl_easy_perform(static_handle);
    curl_easy_getinfo(static_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK)
    {
        LogError("R700 : curl_easy_perform() failed in curlStaticGet: %s (CurlCode %d HttpStatus %ld)",
                 curl_easy_strerror(res),
                 res,
                 http_code);
    }

    *statusCode = (int)http_code;

    if (full_endpoint)
    {
        free(full_endpoint);
    }

    if (http_code == 204)   // No content
    {
        return NULL;
    }
    else
    {
        return session_data->readCallbackData;
    }
}

char* curlStaticPut(
    CURL_Static_Session_Data* session_data,
    char* endpoint,
    char* putData,
    int* statusCode)
{
    CURL* static_handle = session_data->curlHandle;
    long http_code      = 0;

#ifdef DEBUG_CURL
    LogInfo("R700 : %s %s %p", __FUNCTION__, endpoint, putData);
#endif

    // re-initialize callback data buffer
    memset(session_data->readCallbackData, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->readCallbackDataLength = 0;

    memset(session_data->writeCallbackData, '\0', STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->writeCallbackDataLength = 0;

    memcpy(session_data->writeCallbackData, putData, strlen(putData) * sizeof(char));
    session_data->writeCallbackDataLength = strlen(putData);

    char fullurl[1000] = "";

    strcat(fullurl, session_data->basePath);
    strcat(fullurl, endpoint);

    char* full_endpoint = NULL;
    mallocAndStrcpy_s(&full_endpoint, fullurl);

    LogInfo("R700 : PUT Endpoint: %s", full_endpoint);

    curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
    curl_easy_setopt(static_handle, CURLOPT_READFUNCTION, curlStaticDataWriteCallback);
    curl_easy_setopt(static_handle, CURLOPT_READDATA, (void*)session_data);
    curl_easy_setopt(static_handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(static_handle, CURLOPT_POST, 0L);
    curl_easy_setopt(static_handle, CURLOPT_CUSTOMREQUEST, "PUT");

    if (putData)
    {
        curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, putData);
    }

    CURLcode res = curl_easy_perform(static_handle);
    curl_easy_getinfo(static_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK)
    {
        LogError("R700 : curl_easy_perform() failed in curlStaticGet: %s (CurlCode %d HttpStatus %ld)",
                 curl_easy_strerror(res),
                 res,
                 http_code);
    }

    *statusCode = (int)http_code;

    if (full_endpoint)
    {
        free(full_endpoint);
    }

    if (http_code == 204)   // No content
    {
        return NULL;
    }
    else
    {
        return session_data->readCallbackData;
    }
}

char* curlStaticPost(
    CURL_Static_Session_Data* session_data,
    char* endpoint,
    char* postData,
    int* statusCode)
{
    CURL* static_handle = session_data->curlHandle;
    long http_code;
    assert(statusCode != NULL);

#ifdef DEBUG_CURL
    LogInfo("R700 : %s %s", __FUNCTION__, endpoint);
#endif
    // re-initialize callback data buffer
    char* startPtr = session_data->readCallbackData;
    memset(startPtr, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->readCallbackDataLength = 0;

    char fullurl[1000] = "";

    strcat(fullurl, session_data->basePath);
    strcat(fullurl, endpoint);

    char* full_endpoint = NULL;
    mallocAndStrcpy_s(&full_endpoint, fullurl);
    LogInfo("R700 : POST Endpoint: %s", full_endpoint);

    curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
    curl_easy_setopt(static_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(static_handle, CURLOPT_CUSTOMREQUEST, "POST");

    if (postData != NULL)
    {
        curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, postData);
    }
    else
    {
        curl_easy_setopt(static_handle, CURLOPT_POSTFIELDS, "");
    }

    CURLcode res;

    res = curl_easy_perform(static_handle);
    curl_easy_getinfo(static_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK)
    {
        LogError("R700 : curl_easy_perform() failed in curlStaticGet: %s (CurlCode %d HttpStatus %ld)",
                 curl_easy_strerror(res),
                 res,
                 http_code);
    }

    *statusCode = (int)http_code;

    if (full_endpoint)
    {
        free(full_endpoint);
    }

    if (http_code == 204)   // No content
    {
        return NULL;
    }
    else
    {
        return session_data->readCallbackData;
    }
}

char* curlStaticDelete(
    CURL_Static_Session_Data* session_data,
    char* endpoint,
    int* statusCode)
{
    CURL* static_handle = session_data->curlHandle;
    long http_code;

#ifdef DEBUG_CURL
    LogInfo("R700 : %s %s", __FUNCTION__, endpoint);
#endif
    // re-initialize callback data buffer
    char* startPtr = session_data->readCallbackData;
    memset(startPtr, '\0', STATIC_READ_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
    session_data->readCallbackDataLength = 0;

    char fullurl[1000] = "";

    strcat(fullurl, session_data->basePath);
    strcat(fullurl, endpoint);

    char* full_endpoint = NULL;
    mallocAndStrcpy_s(&full_endpoint, fullurl);

    LogInfo("R700 : DELETE Endpoint: %s", full_endpoint);

    curl_easy_setopt(static_handle, CURLOPT_URL, full_endpoint);
    curl_easy_setopt(static_handle, CURLOPT_POST, 0L);
    curl_easy_setopt(static_handle, CURLOPT_CUSTOMREQUEST, "DELETE");

    CURLcode res;

    res = curl_easy_perform(static_handle);
    curl_easy_getinfo(static_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK)
    {
        LogError("R700 : curl_easy_perform() failed in curlStaticDelete: %s (CurlCode %d HttpStatus %ld)",
                 curl_easy_strerror(res),
                 res,
                 http_code);
    }

    *statusCode = (int)http_code;

    if (full_endpoint)
    {
        free(full_endpoint);
    }

    if (http_code == 204)   // No content
    {
        return NULL;
    }
    else
    {
        return session_data->readCallbackData;
    }
}

size_t
curlStaticDataWriteCallback(
    void* write_data,
    size_t size,
    size_t nmemb,
    void* userp)
{
    CURL_Static_Session_Data* session_data = (CURL_Static_Session_Data*)userp;

    int offset = strlen(session_data->writeCallbackData) - session_data->writeCallbackDataLength;   // using writeCallbackDataLength as remaining data to write

    int curlWriteBufferSize = size * nmemb;

    // LogInfo("R700 :     PUT cURL buffer size (bytes): %d", curlWriteBufferSize);
    // LogInfo("R700 :        PUT cURL size: %d", (int)size);
    // LogInfo("R700 :        PUT cURL nmemb: %d", (int)nmemb);
    // LogInfo("R700 :     PUT data size (bytes): %d", session_data->writeCallbackDataLength);
    // LogInfo("R700 :     PUT data to write: %s", session_data->writeCallbackData + offset);

    if (session_data->writeCallbackDataLength * sizeof(char) >= curlWriteBufferSize)
    {   // data to write is larger than cURL buffer, write first chunk

        // LogInfo("R700 :        Data offset: %d", offset);

        memcpy(write_data, (session_data->writeCallbackData) + offset, curlWriteBufferSize);
        session_data->writeCallbackDataLength -= curlWriteBufferSize;   // update remaining data to write
        return curlWriteBufferSize;
    }

    else
    {   // data to write will fit in cURL buffer, write everything
        memcpy(write_data, session_data->writeCallbackData + offset, strlen(session_data->writeCallbackData) * sizeof(char) + 1);

        size_t handled_bytes = strlen(session_data->writeCallbackData) * sizeof(char);

        memset(session_data->writeCallbackData, '\0', STATIC_WRITE_CALLBACK_DATA_BUFFER_SIZE * sizeof(char));
        session_data->writeCallbackDataLength = 0;   // no more data to write

        return handled_bytes;   // must return number of bytes handled.  Return 0 to terminate.
    }
}

void curlStaticCleanup(
    CURL_Static_Session_Data* session_data)
{
    curl_easy_cleanup(session_data->curlHandle);
    free(session_data->readCallbackData);
    free(session_data->writeCallbackData);

    if (session_data->username)
    {
        free(session_data->username);
    }

    if (session_data->password)
    {
        free(session_data->password);
    }

    if (session_data->basePath)
    {
        free(session_data->basePath);
    }

    free(session_data);
}

void curlStreamCleanup(
    CURL_Stream_Session_Data* session_data)
{
    session_data->threadData.thread_ref = pthread_join(session_data->threadData.tid, NULL);
    curl_multi_remove_handle(session_data->multiHandle, session_data->curlHandle);
    curl_multi_cleanup(session_data->multiHandle);
    curl_easy_cleanup(session_data->curlHandle);

    if (session_data->username)
    {
        free(session_data->username);
    }

    if (session_data->password)
    {
        free(session_data->password);
    }

    if (session_data->basePath)
    {
        free(session_data->basePath);
    }

    free(session_data->dataBuffer);
    free(session_data);
}

void curlGlobalCleanup()
{
    curl_global_cleanup();
}
