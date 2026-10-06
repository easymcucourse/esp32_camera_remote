#pragma once
#include <stdbool.h>
typedef struct { char fields[1024]; } cJSON;
cJSON *cJSON_CreateObject(void);
void cJSON_Delete(cJSON*);
char *cJSON_PrintUnformatted(const cJSON*);
void cJSON_AddStringToObject(cJSON*,const char*,const char*);
void cJSON_AddNumberToObject(cJSON*,const char*,double);
void cJSON_AddBoolToObject(cJSON*,const char*,bool);
