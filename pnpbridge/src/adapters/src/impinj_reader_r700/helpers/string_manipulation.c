#include "string_manipulation.h"
#include <stdlib.h>
#include <stdio.h>

Str_Trim_Data 
Str_Trim(
  char *orig_str
  ) 
  {
  
    int strLength = (int)strlen(orig_str);
    char* trimmed_str = malloc(sizeof(char)*(strLength+1));
    strcpy(trimmed_str, orig_str);
  //   trimmed_str[strLength]='\0';  // not needed with strcpy

    Str_Trim_Data *output = malloc(sizeof(Str_Trim_Data));

    output->strPtr = trimmed_str;
    output->strLength = strLength;

    return *output;
  }

