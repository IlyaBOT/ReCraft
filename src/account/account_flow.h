#ifndef RECRAFT_ACCOUNT_FLOW_H
#define RECRAFT_ACCOUNT_FLOW_H
#include "account.h"
#include "../util/https.h"
#include <stdint.h>
typedef struct AccountData {
    char client_id[37],name[17],id[33],skin_id[80],skin_url[512],skin_variant[16];
    char cape_id[80],cape_url[512];
    char msa[8193],refresh[8193],xbox[8193],xsts[8193],minecraft[8193],uhs[64];
    int64_t issued,msa_exp,minecraft_exp;
} AccountData;
typedef struct AccountFlow {
    void *context;
    int (*request)(void *,const char *,const char *,const char *,const char *,HttpsResponse *);
    int (*wait)(void *,unsigned seconds);
    double (*now)(void *); /* Optional monotonic clock; tests may use logical waits. */
    void (*progress)(void *,const char *status,const char *code,const char *uri);
    char error[160];
} AccountFlow;
int account_client_id_valid(const char *id);
int account_flow_run(AccountData *data,AccountFlow *flow,int refresh);
int account_flow_join(const AccountData *data,AccountFlow *flow,const char *server_id);
int account_data_load(AccountData *data,const char *path);
int account_data_save(const AccountData *data,const char *path);
#endif
