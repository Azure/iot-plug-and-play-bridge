#include "string_manipulation.h"
#include <stdio.h>
#include <stdlib.h>

void main() {

    char str_http[] = "http://";
    char str_basepath[] = "/api/v1";

    char build_str_url_always[100] = "";
    strcat(build_str_url_always, str_http);
    strcat(build_str_url_always, "compHostname");
    strcat(build_str_url_always, str_basepath);  

    Str_Trim_Data testOutput = Str_Trim(build_str_url_always);

    printf("\n");
    printf("/n String: %s", (char*)testOutput.strPtr);
    printf("\nSize: %d", testOutput.strLength);

    free(testOutput.strPtr);
    // free(testOutput);

    return;
}