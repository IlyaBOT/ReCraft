#ifndef RECRAFT_JSON_H
#define RECRAFT_JSON_H
#include <stddef.h>
typedef struct RecraftJson RecraftJson;
/* Parsed spans borrow input. Bounded 128 KiB, 2048 tokens, 24 levels. */
RecraftJson *json_parse(const void *data,size_t size);
void json_free(RecraftJson *json);
int json_member(RecraftJson *json,int object,const char *key);
int json_element(RecraftJson *json,int array,int index);
int json_string(RecraftJson *json,int token,char *out,size_t capacity);
int json_integer(RecraftJson *json,int token,int *out);
int json_true(RecraftJson *json,int token);
int json_quote(char *out,size_t capacity,const char *text);
#endif
