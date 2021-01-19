#include <string.h>

typedef struct Str_Trim_Data {
  char* strPtr;
  int strLength;
} Str_Trim_Data;

Str_Trim_Data Str_Trim(char *orig_str);