#include "string_manipulation.h"
#include <stdlib.h>
#include <stdio.h>

char * 
Str_Trim(
  char *orig_str
  ) 
  {
  
    int strLength = (int)strlen(orig_str);
    char* trimmed_str = malloc(sizeof(char)*(strLength+1));
    strcpy(trimmed_str, orig_str);
  //   trimmed_str[strLength]='\0';  // not needed with strcpy

    char * output = trimmed_str;

    return output;
  }

