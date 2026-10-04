#include "player_textures.h"
#include "../util/json.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif
static int uuid_valid(const char *id)
{ return id && strlen(id)==32 && strspn(id,"0123456789abcdefABCDEF")==32; }
static int name_valid(const char *name)
{ return name && *name && strlen(name)<=16 && strspn(name,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==strlen(name); }
static int b64(unsigned c)
{ return c>='A'&&c<='Z'?(int)c-'A':c>='a'&&c<='z'?(int)c-'a'+26:c>='0'&&c<='9'?(int)c-'0'+52:c=='+'?62:c=='/'?63:-1; }
static int texture_url(RecraftJson *j,int textures,const char *key,char *out)
{
    char url[512];const char *hash;int object=json_member(j,textures,key);
    out[0]=0;
    if(!json_string(j,json_member(j,object,"url"),url,sizeof(url)))return 1;
    if(!strncmp(url,"http://textures.minecraft.net/texture/",38))hash=url+38;
    else if(!strncmp(url,"https://textures.minecraft.net/texture/",39))hash=url+39;
    else return 0;
    if(strlen(hash)!=64 || strspn(hash,"0123456789abcdefABCDEF")!=64)return 0;
    snprintf(out,512,"https://textures.minecraft.net/texture/%s",hash);return 1;
}
int player_texture_urls(const void *bytes,size_t size,const char *uuid,char *skin,char *cape)
{
    RecraftJson *j=json_parse(bytes,size),*payload=NULL;char id[33],name[32],encoded[16385],decoded[12289];
    size_t n,i,w=0;int properties,p,ok=0;
    skin[0]=cape[0]=0;
    if(!j || !uuid_valid(uuid) || !json_string(j,json_member(j,0,"id"),id,sizeof(id)) || strcmp(id,uuid))goto done;
    properties=json_member(j,0,"properties");
    for(p=0;p<8;++p) {
        int object=json_element(j,properties,p);
        if(object<0)break;
        if(json_string(j,json_member(j,object,"name"),name,sizeof(name)) && !strcmp(name,"textures") &&
           json_string(j,json_member(j,object,"value"),encoded,sizeof(encoded)))break;
    }
    if(p==8 || json_element(j,properties,p)<0)goto done;
    n=strlen(encoded);if(!n || n%4)goto done;
    for(i=0;i<n;i+=4) {
        int a=b64((unsigned char)encoded[i]),b=b64((unsigned char)encoded[i+1]);
        int c=encoded[i+2]=='='?-2:b64((unsigned char)encoded[i+2]),d=encoded[i+3]=='='?-2:b64((unsigned char)encoded[i+3]);unsigned bits;
        if(a<0 || b<0 || c==-1 || d==-1 || (c==-2 && d!=-2) || ((c==-2 || d==-2) && i+4!=n) ||
           (c==-2 && (b&15)) || (d==-2 && c>=0 && (c&3)))goto done;
        bits=((unsigned)a<<18)|((unsigned)b<<12)|((unsigned)(c>=0?c:0)<<6)|(unsigned)(d>=0?d:0);
        decoded[w++]=(char)(bits>>16);if(c>=0)decoded[w++]=(char)(bits>>8);if(d>=0)decoded[w++]=(char)bits;
    }
    payload=json_parse(decoded,w);
    if(!payload || !json_string(payload,json_member(payload,0,"profileId"),id,sizeof(id)) || strcmp(id,uuid))goto done;
    p=json_member(payload,0,"textures");ok=texture_url(payload,p,"SKIN",skin) && texture_url(payload,p,"CAPE",cape);
done:
    if(!ok)skin[0]=cape[0]=0;
    json_free(payload);json_free(j);return ok;
}
void player_texture_result_free(PlayerTextureResult *r)
{ free(r->skin);free(r->cape);memset(r,0,sizeof(*r)); }
int player_texture_fetch(const char *name,const char *uuid,PlayerTextureRequest request,void *context,PlayerTextureResult *out)
{
    char url[160],id[33]={0},skin[512],cape[512];HttpsResponse r={0};RecraftJson *j;int ok=0;
    memset(out,0,sizeof(*out));
    if(!request)return 0;
    if(uuid && *uuid) {if(!uuid_valid(uuid))return 0;strcpy(id,uuid);}
    else {
        if(!name_valid(name))return 0;
        snprintf(url,sizeof(url),"https://api.mojang.com/users/profiles/minecraft/%s",name);
        if(!request(context,url,&r) || r.status!=200)goto done;
        j=json_parse(r.data,r.size);ok=json_string(j,json_member(j,0,"id"),id,sizeof(id));json_free(j);https_response_free(&r);
        if(!ok || !uuid_valid(id)){ok=0;goto done;}
    }
    snprintf(url,sizeof(url),"https://sessionserver.mojang.com/session/minecraft/profile/%s",id);
    if(!request(context,url,&r) || r.status!=200 || !player_texture_urls(r.data,r.size,id,skin,cape)) {ok=0;goto done;}
    strcpy(out->id,id);https_response_free(&r);ok=1;
    if(*skin && request(context,skin,&r) && r.status==200 && r.size<=65536) {out->skin=r.data;out->skin_size=r.size;r.data=NULL;}
    https_response_free(&r);
    if(*cape && request(context,cape,&r) && r.status==200 && r.size<=65536) {out->cape=r.data;out->cape_size=r.size;r.data=NULL;}
done:
    https_response_free(&r);return ok;
}
struct PlayerTextures {
    struct { char name[17],id[33]; int state; } entries[PLAYER_TEXTURE_LIMIT];
    PlayerTextureResult result;int started,done,cancelled,active;
#ifdef _WIN32
    CRITICAL_SECTION lock;HANDLE thread;
#else
    pthread_mutex_t lock;pthread_t thread;
#endif
};
static void take(PlayerTextures *p) {
#ifdef _WIN32
    EnterCriticalSection(&p->lock);
#else
    pthread_mutex_lock(&p->lock);
#endif
}
static void drop(PlayerTextures *p) {
#ifdef _WIN32
    LeaveCriticalSection(&p->lock);
#else
    pthread_mutex_unlock(&p->lock);
#endif
}
static int cancelled(void *context)
{ PlayerTextures *p=context;int value;take(p);value=p->cancelled;drop(p);return value; }
static int request(void *context,const char *url,HttpsResponse *r)
{ return https_request(url,NULL,NULL,NULL,r,cancelled,context); }
#ifdef _WIN32
static unsigned __stdcall worker(void *context)
#else
static void *worker(void *context)
#endif
{
    PlayerTextures *p=context;PlayerTextureResult r;int i=p->active;
    player_texture_fetch(p->entries[i].name,p->entries[i].id,request,p,&r);r.slot=(unsigned)i;
    take(p);p->result=r;p->done=1;drop(p);return 0;
}
static void join(PlayerTextures *p)
{
    if(!p->started)return;
#ifdef _WIN32
    WaitForSingleObject(p->thread,INFINITE);CloseHandle(p->thread);p->thread=NULL;
#else
    pthread_join(p->thread,NULL);
#endif
    p->started=0;
}
PlayerTextures *player_textures_create(void)
{
    PlayerTextures *p=calloc(1,sizeof(*p));if(!p)return NULL;
#ifdef _WIN32
    InitializeCriticalSection(&p->lock);
#else
    pthread_mutex_init(&p->lock,NULL);
#endif
    return p;
}
void player_textures_destroy(PlayerTextures *p)
{
    if(!p)return;
    take(p);p->cancelled=1;drop(p);join(p);player_texture_result_free(&p->result);
#ifdef _WIN32
    DeleteCriticalSection(&p->lock);
#else
    pthread_mutex_destroy(&p->lock);
#endif
    free(p);
}
int player_textures_queue(PlayerTextures *p,const char *name,const char *uuid)
{
    int i,empty=-1;if(!p || ((uuid && *uuid) ? !uuid_valid(uuid) : !name_valid(name)))return -1;
    for(i=0;i<PLAYER_TEXTURE_LIMIT;++i) {
        if(!p->entries[i].state) {if(empty<0)empty=i;continue;}
        if(uuid && *uuid ? !strcmp(p->entries[i].id,uuid) : !strcmp(p->entries[i].name,name))return i;
    }
    if(empty<0)return -1;
    snprintf(p->entries[empty].name,sizeof(p->entries[empty].name),"%s",name ? name : "");
    snprintf(p->entries[empty].id,sizeof(p->entries[empty].id),"%s",uuid ? uuid : "");p->entries[empty].state=1;return empty;
}
int player_textures_poll(PlayerTextures *p,PlayerTextureResult *out)
{
    int ready=0,i;if(!p)return 0;
    take(p);if(p->done){*out=p->result;memset(&p->result,0,sizeof(p->result));p->done=0;ready=1;}drop(p);
    if(ready){join(p);p->entries[out->slot].state=2;}
    if(!p->started) for(i=0;i<PLAYER_TEXTURE_LIMIT;++i) if(p->entries[i].state==1) {
        p->active=i;
#ifdef _WIN32
        p->thread=(HANDLE)_beginthreadex(NULL,0,worker,p,0,NULL);p->started=p->thread!=NULL;
#else
        p->started=pthread_create(&p->thread,NULL,worker,p)==0;
#endif
        if(!p->started)p->entries[i].state=2;
        break;
    }
    return ready;
}
