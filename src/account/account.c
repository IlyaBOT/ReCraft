#include "account_flow.h"
#include "../util/json.h"
#include "../util/clock.h"
#include "../util/file_dialog.h"
#include "../util/game_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef RECRAFT_ACCOUNT_USE_BUILD_DEFAULT
#include "recraft_version.h"
#else
/* Headless tests deliberately have no production application or network flow. */
#define RECRAFT_MICROSOFT_CLIENT_ID_DEFAULT ""
#endif
#ifdef _WIN32
#include <windows.h>
#include <process.h>
typedef CRITICAL_SECTION AccountLock;
static void lock_init(AccountLock *l){InitializeCriticalSection(l);}
static void lock_take(AccountLock *l){EnterCriticalSection(l);}
static void lock_drop(AccountLock *l){LeaveCriticalSection(l);}
static void lock_free(AccountLock *l){DeleteCriticalSection(l);}
#else
#include <pthread.h>
typedef pthread_mutex_t AccountLock;
static void lock_init(AccountLock *l){pthread_mutex_init(l,NULL);}
static void lock_take(AccountLock *l){pthread_mutex_lock(l);}
static void lock_drop(AccountLock *l){pthread_mutex_unlock(l);}
static void lock_free(AccountLock *l){pthread_mutex_destroy(l);}
#endif
struct Account {
    AccountLock lock;AccountView view;AccountData data;
    char path[1024];int cancelled,started;
    unsigned char *skin;size_t skin_size;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
};
static int cancelled(void *context)
{Account *a=(Account *)context;int value;lock_take(&a->lock);value=a->cancelled;lock_drop(&a->lock);return value;}
static int request(void *context,const char *url,const char *type,const char *body,const char *token,HttpsResponse *r)
{return !cancelled(context)&&https_request(url,type,body,token,r,cancelled,context);}
static int wait_code(void *context,unsigned seconds)
{double until=recraft_now_seconds()+seconds;while(recraft_now_seconds()<until){if(cancelled(context))return 0;recraft_sleep_seconds(.1);}return !cancelled(context);}
static double now(void *context){(void)context;return recraft_now_seconds();}
static void progress(void *context,const char *status,const char *code,const char *uri)
{
    Account *a=(Account *)context;lock_take(&a->lock);
    snprintf(a->view.status,sizeof(a->view.status),"%s",status);snprintf(a->view.code,sizeof(a->view.code),"%s",code);
    snprintf(a->view.verification_uri,sizeof(a->view.verification_uri),"%s",uri);++a->view.revision;lock_drop(&a->lock);
}
static void run(Account *a)
{
    AccountData *data=(AccountData *)malloc(sizeof(*data));AccountFlow flow={0};HttpsResponse skin={0};int ok=0;
    if(!data){progress(a,"Not enough memory for sign-in.","","");goto done;}
    lock_take(&a->lock);*data=a->data;lock_drop(&a->lock);
    flow.context=a;flow.request=request;flow.wait=wait_code;flow.now=now;flow.progress=progress;
    ok=account_flow_run(data,&flow,data->refresh[0]!=0);
    if(cancelled(a)){ok=0;snprintf(flow.error,sizeof(flow.error),"Sign-in cancelled.");}
    if(ok&&!account_data_save(data,a->path)){ok=0;snprintf(flow.error,sizeof(flow.error),"Unable to save account. Previous account was kept.");}
    if(ok && data->skin_url[0]) request(a,data->skin_url,NULL,NULL,NULL,&skin);
    lock_take(&a->lock);
    if(ok) {
        a->data=*data;a->view.signed_in=1;
        snprintf(a->view.name,sizeof(a->view.name),"%s",data->name);snprintf(a->view.id,sizeof(a->view.id),"%s",data->id);
        snprintf(a->view.status,sizeof(a->view.status),"Signed in as %s.%s",data->name,skin.status==200?"":" Skin download unavailable.");
        if(skin.status==200&&skin.size<=65536){free(a->skin);a->skin=skin.data;a->skin_size=skin.size;skin.data=NULL;}
    } else snprintf(a->view.status,sizeof(a->view.status),"%s",flow.error);
    a->view.code[0]=a->view.verification_uri[0]=0;++a->view.revision;lock_drop(&a->lock);
    https_response_free(&skin);memset(data,0,sizeof(*data));free(data);
done:
    lock_take(&a->lock);a->view.busy=0;++a->view.revision;lock_drop(&a->lock);
}
#ifdef _WIN32
static unsigned __stdcall worker(void *context){run((Account *)context);return 0;}
#else
static void *worker(void *context){run((Account *)context);return NULL;}
#endif
static void join(Account *a)
{
    if(!a->started)return;
#ifdef _WIN32
    WaitForSingleObject(a->thread,INFINITE);CloseHandle(a->thread);a->thread=NULL;
#else
    pthread_join(a->thread,NULL);
#endif
    a->started=0;
}
Account *account_create(const char *root)
{
    Account *a=(Account *)calloc(1,sizeof(*a));char path[1024],client[37]={0};void *bytes;size_t size;
    const char *environment=getenv("RECRAFT_MICROSOFT_CLIENT_ID");
    if(!a)return NULL;
    lock_init(&a->lock);
    if(!game_path_join(a->path,sizeof(a->path),root,"config/accounts.json")){account_destroy(a);return NULL;}
    account_data_load(&a->data,a->path);
    if(account_client_id_valid(environment))snprintf(client,sizeof(client),"%s",environment);
    else if(game_path_join(path,sizeof(path),root,"config/microsoft_auth.json")&&(bytes=game_read_small_file(path,1024,&size))) {
        RecraftJson *j=json_parse(bytes,size);json_string(j,json_member(j,0,"client_id"),client,sizeof(client));json_free(j);free(bytes);
    }
    if(!account_client_id_valid(client) && !a->data.client_id[0])
        snprintf(client,sizeof(client),"%s",RECRAFT_MICROSOFT_CLIENT_ID_DEFAULT);
    if(account_client_id_valid(client)) {
        if(a->data.client_id[0]&&strcmp(a->data.client_id,client))memset(&a->data,0,sizeof(a->data));
        strcpy(a->data.client_id,client);
    }
    if(a->data.name[0]) {
        strcpy(a->view.name,a->data.name);strcpy(a->view.id,a->data.id);a->view.signed_in=1;
        strcpy(a->view.status,"Saved account; Sign in refreshes its session.");
    } else strcpy(a->view.status,"Offline profile. Microsoft sign-in is optional.");
    a->view.revision=1;return a;
}
void account_destroy(Account *a)
{if(!a)return;account_cancel(a);join(a);free(a->skin);lock_free(&a->lock);memset(a,0,sizeof(*a));free(a);}
void account_cancel(Account *a)
{if(!a)return;lock_take(&a->lock);a->cancelled=1;if(a->view.busy){strcpy(a->view.status,"Cancelling sign-in...");++a->view.revision;}lock_drop(&a->lock);}
int account_sign_in(Account *a)
{
    int busy;if(!a)return 0;lock_take(&a->lock);busy=a->view.busy;lock_drop(&a->lock);if(busy)return 0;
    join(a);lock_take(&a->lock);a->cancelled=0;a->view.busy=1;++a->view.revision;lock_drop(&a->lock);
#ifdef _WIN32
    a->thread=(HANDLE)_beginthreadex(NULL,0,worker,a,0,NULL);a->started=a->thread!=NULL;
#else
    a->started=pthread_create(&a->thread,NULL,worker,a)==0;
#endif
    if(!a->started){lock_take(&a->lock);a->view.busy=0;strcpy(a->view.status,"Unable to start sign-in worker.");++a->view.revision;lock_drop(&a->lock);}
    return a->started;
}
int account_sign_out(Account *a)
{
    if(!a)return 0;
    account_cancel(a);join(a);
    if(remove(a->path)&&errno!=ENOENT)return 0;
    lock_take(&a->lock);
    {char client[37];strcpy(client,a->data.client_id);memset(&a->data,0,sizeof(a->data));strcpy(a->data.client_id,client);}
    {unsigned revision=a->view.revision+1;memset(&a->view,0,sizeof(a->view));a->view.revision=revision;}
    strcpy(a->view.status,"Signed out. Offline profile is available.");
    free(a->skin);a->skin=NULL;a->skin_size=0;lock_drop(&a->lock);return 1;
}
void account_view(Account *a,AccountView *view)
{memset(view,0,sizeof(*view));if(a){lock_take(&a->lock);*view=a->view;lock_drop(&a->lock);}}
unsigned char *account_take_skin(Account *a,size_t *size)
{unsigned char *png;*size=0;if(!a)return NULL;lock_take(&a->lock);png=a->skin;*size=a->skin_size;a->skin=NULL;a->skin_size=0;lock_drop(&a->lock);return png;}
