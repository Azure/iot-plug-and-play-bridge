#include "common.h"

//
// Note: Allows 256 keys
//

MAP_RESULT Map_Add_Index(MAP_HANDLE handle, const char* key, int value) {
	char a[2] = { 0 };
	a[0] = value;
	return Map_Add(handle, key, a);
}

int Map_GetIndexValueFromKey(MAP_HANDLE handle, const char* key) {
	const char* index = Map_GetValueFromKey(handle, key);
	return index[0];
}