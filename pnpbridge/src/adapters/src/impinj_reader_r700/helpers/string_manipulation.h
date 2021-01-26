#include <string.h>

typedef struct Str_Trim_Data {
  char* strPtr;
  int strLength;
} Str_Trim_Data;

typedef struct Str_Split_Data {
  char * firstStr;
  char * secondStr;
} Str_Split_Data;

Str_Trim_Data 
Str_Trim(
  char *orig_str
  );

Str_Split_Data 
Str_Split (
  char * orig_str,
  char * delimiter
  );