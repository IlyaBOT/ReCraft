#ifndef RECRAFT_ACCOUNT_H
#define RECRAFT_ACCOUNT_H
#include <stddef.h>
typedef struct Account Account;
typedef struct AccountView {
    char name[17],id[33],code[32],verification_uri[128],status[160];
    int busy,signed_in;unsigned revision;
} AccountView;
Account *account_create(const char *root);
void account_destroy(Account *account);
int account_sign_in(Account *account);
void account_cancel(Account *account);
int account_sign_out(Account *account);
void account_view(Account *account,AccountView *view);
/* Transfer downloaded skin ownership to the render thread, once. */
unsigned char *account_take_skin(Account *account,size_t *size);
#endif
