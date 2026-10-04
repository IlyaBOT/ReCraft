#include "account/account_flow.h"
#include "account/player_textures.h"
#include "util/json.h"
#include "util/clock.h"
#include "util/game_paths.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_pid() _getpid()
#define test_rmdir(path) _rmdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#define test_pid() getpid()
#define test_rmdir(path) rmdir(path)
#endif

/* Dummy tokens, no network or real credentials. Check the request contract as
 * well as responses: device polling, refresh rotation, Xbox and MC audiences. */
static const char *client="12345678-1234-1234-1234-123456789abc";
static const char *profile="{\"id\":\"0123456789abcdef0123456789abcdef\",\"name\":\"BetaPlayer\",\"skins\":[{\"id\":\"skin-id\",\"state\":\"ACTIVE\",\"variant\":\"CLASSIC\",\"url\":\"http://textures.minecraft.net/texture/test\"}],\"capes\":[{\"id\":\"cape-test\",\"state\":\"INACTIVE\",\"url\":\"https://textures.minecraft.net/texture/inactive\"},{\"id\":\"active-cape\",\"state\":\"ACTIVE\",\"url\":\"http://textures.minecraft.net/texture/cape\"}]}";
typedef struct Mock {
    int stage,polls,waits,cancel,fail_stage,reject_refresh,malformed,device_size,fail_status;
    unsigned interval[4];
    const char *error_body;
} Mock;
static int respond(HttpsResponse *r,long status,const char *body)
{
    r->size=strlen(body);r->status=status;r->data=(unsigned char *)malloc(r->size+1);
    assert(r->data);memcpy(r->data,body,r->size+1);return 1;
}
static int mock_request(void *context,const char *url,const char *type,const char *body,const char *bearer,HttpsResponse *r)
{
    Mock *m=(Mock *)context;int stage;
    if(strstr(url,"/devicecode")) {
        stage=1;assert(type&&!strcmp(type,"application/x-www-form-urlencoded")&&!bearer);
        assert(strstr(body,client)&&strstr(body,"XboxLive.SignIn%20XboxLive.offline_access"));
    } else if(strstr(url,"/token")) {
        stage=2;assert(type&&strstr(body,client));
        if(strstr(body,"grant_type=refresh_token")) {
            assert(strstr(body,"refresh_token=old%2Brefresh"));
            if(m->reject_refresh){m->reject_refresh=0;return respond(r,400,"{\"error\":\"invalid_grant\"}");}
        } else {
            if(m->device_size) {
                const char *begin=strstr(body,"device_code=");assert(begin);begin+=12;
                assert(strchr(begin,'&')-begin==m->device_size&&begin[0]=='x');
            } else assert(strstr(body,"device_code=device%2Btest"));
            if(m->polls++==0)return respond(r,400,"{\"error\":\"authorization_pending\"}");
            if(m->polls==2)return respond(r,400,"{\"error\":\"slow_down\"}");
        }
    } else if(strstr(url,"/user/authenticate")) {
        stage=3;assert(type&&!strcmp(type,"application/json"));
        assert(strstr(body,"\"RpsTicket\":\"d=msa-test\"")&&strstr(body,"http://auth.xboxlive.com"));
    } else if(strstr(url,"/xsts/authorize")) {
        stage=4;assert(strstr(body,"rp://api.minecraftservices.com/")&&strstr(body,"xbox-test"));
    } else if(strstr(url,"/launcher/login")) {
        stage=5;assert(strstr(body,"\"xtoken\":\"XBL3.0 x=12345;xsts-test\"")&&strstr(body,"PC_LAUNCHER"));
    } else if(strstr(url,"/session/minecraft/join")) {
        assert(type&&!strcmp(type,"application/json")&&!bearer);
        assert(strstr(body,"\"accessToken\":\"mc-test\"")&&strstr(body,"\"selectedProfile\":\"0123456789abcdef0123456789abcdef\"")&&strstr(body,"\"serverId\":\"-abc123\""));
        return respond(r,m->fail_stage ? 403 : 204,"");
    } else {
        stage=6;assert(!strcmp(url,"https://api.minecraftservices.com/minecraft/profile"));
        assert(!body&&!type&&bearer&&!strcmp(bearer,"mc-test"));
    }
    m->stage=stage;
    if(m->fail_stage==stage)return respond(r,m->fail_status?m->fail_status:stage==2?400:403,m->error_body?m->error_body:"{}");
    if(m->malformed==stage)return respond(r,200,"{invalid");
    switch(stage) {
    case 1:
        if(m->device_size) {
            char device[8193],response[9000];assert(m->device_size<=8192);
            memset(device,'x',(size_t)m->device_size);device[m->device_size]=0;
            snprintf(response,sizeof(response),"{\"device_code\":\"%s\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://www.microsoft.com/link\",\"interval\":5,\"expires_in\":900}",device);
            return respond(r,200,response);
        }
        return respond(r,200,"{\"device_code\":\"device+test\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://www.microsoft.com/link\",\"interval\":5,\"expires_in\":900}");
    case 2:return respond(r,200,"{\"access_token\":\"msa-test\",\"refresh_token\":\"rotated-refresh\",\"expires_in\":3600}");
    case 3:return respond(r,200,"{\"Token\":\"xbox-test\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"12345\"}]}}");
    case 4:return respond(r,200,"{\"Token\":\"xsts-test\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"12345\"}]}}");
    case 5:return respond(r,200,"{\"access_token\":\"mc-test\",\"expires_in\":86400}");
    default:return respond(r,200,profile);
    }
}
static int mock_wait(void *context,unsigned seconds)
{
    Mock *m=(Mock *)context;assert(m->waits<4);m->interval[m->waits++]=seconds;return !m->cancel;
}
static void mock_progress(void *context,const char *status,const char *code,const char *uri)
{
    (void)context;assert(status&&*status);
    if(*code)assert(!strcmp(code,"ABCD-EFGH")&&!strcmp(uri,"https://www.microsoft.com/link"));
}
static AccountFlow flow_for(Mock *m)
{AccountFlow f={0};f.context=m;f.request=mock_request;f.wait=mock_wait;f.progress=mock_progress;return f;}
static void fresh(AccountData *d)
{memset(d,0,sizeof(*d));strcpy(d->client_id,client);}
static void flows(AccountData *d)
{
    Mock m={0};AccountFlow f=flow_for(&m);int i;
    fresh(d);assert(account_flow_run(d,&f,0));
    assert(m.stage==6&&m.waits==3&&m.interval[0]==5&&m.interval[1]==5&&m.interval[2]==10);
    assert(!strcmp(d->name,"BetaPlayer")&&!strcmp(d->refresh,"rotated-refresh"));
    assert(!strcmp(d->skin_url,"https://textures.minecraft.net/texture/test"));
    assert(!strcmp(d->cape_url,"https://textures.minecraft.net/texture/cape")&&!strcmp(d->cape_id,"active-cape"));
    assert(account_flow_join(d,&f,"-abc123"));m.fail_stage=1;
    assert(!account_flow_join(d,&f,"-abc123")&&strstr(f.error,"HTTP 403"));m.fail_stage=0;
    assert(!account_flow_join(d,&f,"bad/id"));
    assert(d->msa_exp>d->issued&&d->minecraft_exp>d->issued);
    memset(&m,0,sizeof(m));strcpy(d->refresh,"old+refresh");assert(account_flow_run(d,&f,1)&&!m.waits);
    memset(&m,0,sizeof(m));m.reject_refresh=1;strcpy(d->refresh,"old+refresh");assert(account_flow_run(d,&f,1)&&m.waits==3);
    for(i=1;i<=6;++i) {
        memset(&m,0,sizeof(m));m.fail_stage=i;fresh(d);assert(!account_flow_run(d,&f,0)&&f.error[0]);
        memset(&m,0,sizeof(m));m.malformed=i;fresh(d);assert(!account_flow_run(d,&f,0)&&f.error[0]);
    }
    memset(&m,0,sizeof(m));m.fail_stage=2;m.error_body="{\"error\":\"authorization_declined\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"declined"));
    memset(&m,0,sizeof(m));m.fail_stage=2;m.error_body="{\"error\":\"expired_token\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"expired"));
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="{\"errorMessage\":\"Invalid app registration\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"HTTP 403")&&strstr(f.error,"app registration"));
    assert(strstr(f.error,"aka.ms/mce-reviewappid")&&m.stage==5);
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="{\"message\":\"INVALID APP REGISTRATION\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"app registration"));
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="{\"errorMessage\":\"App registration is valid; access denied\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"cause not identified")&&!strstr(f.error,"app registration"));
    /* A bare 403 at token exchange cannot diagnose game ownership or app approval. */
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="{\"error\":\"FORBIDDEN\",\"access_token\":\"secret-token\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"login HTTP 403")&&strstr(f.error,"cause not identified"));
    assert(!strstr(f.error,"entitlement")&&!strstr(f.error,"app registration")&&!strstr(f.error,"secret-token")&&m.stage==5);
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="{\"error\":\"secret-token\",\"errorMessage\":\"secret-refresh\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&!strstr(f.error,"secret-"));
    memset(&m,0,sizeof(m));m.fail_stage=5;m.error_body="<html>403 Forbidden</html>";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"login HTTP 403"));
    memset(&m,0,sizeof(m));m.fail_stage=5;m.fail_status=503;m.error_body="{\"errorMessage\":\"Invalid app registration\"}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"HTTP 503")&&strstr(f.error,"service unavailable"));
    memset(&m,0,sizeof(m));m.fail_stage=6;m.fail_status=404;fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"profile HTTP 404")&&strstr(f.error,"no Java profile"));
    memset(&m,0,sizeof(m));m.fail_stage=6;m.fail_status=401;fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"profile HTTP 401")&&strstr(f.error,"token expired"));
    memset(&m,0,sizeof(m));m.fail_stage=6;fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"profile HTTP 403")&&!strstr(f.error,"ownership"));
    memset(&m,0,sizeof(m));m.malformed=6;fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"invalid Java profile response"));
    memset(&m,0,sizeof(m));m.cancel=1;fresh(d);assert(!account_flow_run(d,&f,0)&&strstr(f.error,"cancelled"));
    memset(&m,0,sizeof(m));m.device_size=1045;fresh(d);assert(account_flow_run(d,&f,0));
    memset(&m,0,sizeof(m));m.device_size=8192;fresh(d);assert(!account_flow_run(d,&f,0)&&strstr(f.error,"oversized"));
    memset(&m,0,sizeof(m));m.fail_stage=1;m.error_body="{\"error\":\"invalid_client\",\"error_codes\":[7000218]}";fresh(d);
    assert(!account_flow_run(d,&f,0)&&strstr(f.error,"AADSTS7000218"));
    memset(&m,0,sizeof(m));fresh(d);assert(account_flow_run(d,&f,0));
}
typedef struct TextureMock {char profile[4096];int calls,fail;} TextureMock;
static int texture_request(void *context,const char *url,HttpsResponse *r)
{
    TextureMock *m=context;const char *body="PNG";++m->calls;memset(r,0,sizeof(*r));r->status=200;
    if(strstr(url,"/users/profiles/"))body="{\"id\":\"0123456789abcdef0123456789abcdef\",\"name\":\"BetaPlayer\"}";
    else if(strstr(url,"/session/minecraft/profile/"))body=m->profile;
    else assert(!strncmp(url,"https://textures.minecraft.net/texture/",39));
    if(m->fail)r->status=404;
    r->size=strlen(body);r->data=malloc(r->size+1);assert(r->data);memcpy(r->data,body,r->size+1);return 1;
}
static void public_textures_test(void)
{
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const char *uuid="0123456789abcdef0123456789abcdef",*hash="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char payload[2048],encoded[3000],skin[512],cape[512];size_t n,i,w=0;TextureMock m={0};PlayerTextureResult r;PlayerTextures *queue;
    snprintf(payload,sizeof(payload),"{\"profileId\":\"%s\",\"textures\":{\"SKIN\":{\"url\":\"http://textures.minecraft.net/texture/%s\"},\"CAPE\":{\"url\":\"https://textures.minecraft.net/texture/%s\"}}}",uuid,hash,hash);
    n=strlen(payload);
    for(i=0;i<n;i+=3){unsigned bits=(unsigned char)payload[i]<<16;if(i+1<n)bits|=(unsigned char)payload[i+1]<<8;if(i+2<n)bits|=(unsigned char)payload[i+2];
        encoded[w++]=alphabet[bits>>18];encoded[w++]=alphabet[(bits>>12)&63];encoded[w++]=i+1<n?alphabet[(bits>>6)&63]:'=';encoded[w++]=i+2<n?alphabet[bits&63]:'=';}
    encoded[w]=0;snprintf(m.profile,sizeof(m.profile),"{\"id\":\"%s\",\"properties\":[{\"name\":\"textures\",\"value\":\"%s\"}]}",uuid,encoded);
    assert(player_texture_urls(m.profile,strlen(m.profile),uuid,skin,cape) && !strncmp(skin,"https://",8) && !strcmp(skin,cape));
    assert(!player_texture_urls(m.profile,strlen(m.profile),"ffffffffffffffffffffffffffffffff",skin,cape));
    assert(player_texture_fetch("BetaPlayer",NULL,texture_request,&m,&r) && m.calls==4 && r.skin_size==3 && r.cape_size==3 && !strcmp(r.id,uuid));player_texture_result_free(&r);
    m.calls=0;assert(player_texture_fetch(NULL,uuid,texture_request,&m,&r) && m.calls==3);player_texture_result_free(&r);
    m.calls=0;assert(!player_texture_fetch("bad/name",NULL,texture_request,&m,&r) && !m.calls);
    m.fail=1;assert(!player_texture_fetch("BetaPlayer",NULL,texture_request,&m,&r));player_texture_result_free(&r);
    queue=player_textures_create();assert(queue && player_textures_queue(queue,"BetaPlayer",NULL)==0 && player_textures_queue(queue,"BetaPlayer",NULL)==0);
    assert(player_textures_queue(queue,NULL,uuid)==1 && player_textures_queue(queue,"bad/name",NULL)==-1);player_textures_destroy(queue);
    puts("Public name/UUID texture resolution, skin/cape payload bounds and queue deduplication passed");
}
static void json_tests(void)
{
    const char *good="{\"escaped\":\"A\\u0411\\ud83d\\ude00\",\"array\":[true,42]}";
    const char *bad[]={"{\"x\":01}","{\"x\":\"\\ud800\"}","[true,]","{}{}","{\"x\":\"\\u0000\"}"};
    RecraftJson *j=json_parse(good,strlen(good));char out[32];int value;unsigned i;
    assert(j&&json_string(j,json_member(j,0,"escaped"),out,sizeof(out))&&!strcmp(out,"A\xd0\x91\xf0\x9f\x98\x80"));
    assert(json_true(j,json_element(j,json_member(j,0,"array"),0)));
    assert(json_integer(j,json_element(j,json_member(j,0,"array"),1),&value)&&value==42);json_free(j);
    for(i=0;i<4;++i)assert(!json_parse(bad[i],strlen(bad[i])));
    j=json_parse(bad[4],strlen(bad[4]));assert(j&&!json_string(j,json_member(j,0,"x"),out,sizeof(out)));json_free(j);
    assert(!json_quote(out,sizeof(out),"a\nb")&&json_quote(out,sizeof(out),"a\"b\\c"));
    assert(!account_client_id_valid(NULL)&&!account_client_id_valid("74658136")&&account_client_id_valid(client));
}
static void store_and_manager(AccountData *d)
{
    char root[128],config[160],path[180],temp[220];AccountData *loaded=(AccountData *)calloc(1,sizeof(*loaded));
    Account *a;AccountView before,after;FILE *file;
    snprintf(root,sizeof(root),"account-fixture-%ld",(long)test_pid());assert(game_ensure_directory(root));
    snprintf(config,sizeof(config),"%s/config",root);assert(game_ensure_directory(config));
    snprintf(path,sizeof(path),"%s/accounts.json",config);assert(account_data_save(d,path));assert(account_data_load(loaded,path));
    assert(!strcmp(loaded->refresh,d->refresh)&&!strcmp(loaded->name,d->name)&&!strcmp(loaded->minecraft,d->minecraft));
    assert(!strcmp(loaded->cape_url,d->cape_url)&&!strcmp(loaded->cape_id,d->cape_id));
#ifndef _WIN32
    {struct stat s;assert(!stat(path,&s)&&(s.st_mode&0777)==0600);}
#endif
    /* A pre-existing temporary file cannot overwrite the saved account. */
    snprintf(temp,sizeof(temp),"%s.tmp-%ld",path,(long)test_pid());file=fopen(temp,"wb");assert(file);fputs("reserved",file);fclose(file);
    strcpy(d->name,"Changed");assert(!account_data_save(d,path));assert(account_data_load(loaded,path)&&!strcmp(loaded->name,"BetaPlayer"));
    assert(!remove(temp));
    a=account_create(root);assert(a);account_view(a,&before);assert(before.signed_in&&!strcmp(before.name,"BetaPlayer"));
    assert(account_sign_out(a));account_view(a,&after);assert(!after.signed_in&&after.revision>before.revision);account_destroy(a);
    assert(!account_data_load(loaded,path));
    a=account_create(root);assert(a&&account_sign_in(a));
    {double limit=recraft_now_seconds()+3;do{account_view(a,&after);if(!after.busy)break;recraft_sleep_seconds(.01);}while(recraft_now_seconds()<limit);}
    assert(!after.busy&&!after.signed_in&&strstr(after.status,"client_id"));account_destroy(a);
    assert(!test_rmdir(config)&&!test_rmdir(root));memset(loaded,0,sizeof(*loaded));free(loaded);
}
int main(void)
{
    AccountData *d=(AccountData *)calloc(1,sizeof(*d));assert(d);
#ifdef _WIN32
    _putenv("RECRAFT_MICROSOFT_CLIENT_ID=");
#else
    unsetenv("RECRAFT_MICROSOFT_CLIENT_ID");
#endif
    json_tests();public_textures_test();flows(d);store_and_manager(d);memset(d,0,sizeof(*d));free(d);
    puts("Device/refresh flow, Xbox/MC contracts, failures, cancellation and private atomic account persistence passed");return 0;
}
