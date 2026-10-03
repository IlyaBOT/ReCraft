#ifndef RECRAFT_HTTPS_H
#define RECRAFT_HTTPS_H
#include <stddef.h>
typedef struct HttpsResponse { unsigned char *data;size_t size;int status; } HttpsResponse;
typedef int (*HttpsCancelled)(void *context);
/* Verified HTTPS via an optional curl helper; no secret command arguments or
 * temporary request files. The caller must use an asynchronous worker. */
int https_request(const char *url,const char *content_type,const char *body,const char *bearer,
                  HttpsResponse *response,HttpsCancelled cancelled,void *context);
void https_response_free(HttpsResponse *response);
#endif
