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

Str_Split_Data
Str_Split(   // do not use, just use strtok()
  char * orig_str,
  char * delimiter
)
{
  Str_Split_Data output;
   
  char * token = strtok(orig_str, "\n\r");

  int count = 0;

  while ( token != NULL) {
    count++;
    fprintf(stdout, "\nToken %d: %s", count, token);
    token = strtok(NULL, "\n\r");
  }

  return output;
}