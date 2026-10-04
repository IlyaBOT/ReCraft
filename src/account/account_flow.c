#include "account_flow.h"
#include "../util/json.h"
#include "../game/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int uuid32(const char *id);
int account_client_id_valid(const char *id)
{
    int i;if(!id||strlen(id)!=36)return 0;
    for(i=0;i<36;++i) {
        if(i==8||i==13||i==18||i==23){if(id[i]!='-')return 0;}
        else if(!strchr("0123456789abcdefABCDEF",id[i]))return 0;
    }
    return 1;
}
int account_flow_join(const AccountData *d,AccountFlow *f,const char *server_id)
{
    char *body=(char *)calloc(20000,1),*quoted=(char *)malloc(18000),server[140];HttpsResponse r={0};int ok=0;
    f->error[0]=0;
    if(d->minecraft[0] && uuid32(d->id) && server_id && strlen(server_id)<=64 && server_id[0] &&
       strspn(server_id,"-0123456789abcdefABCDEF")==strlen(server_id) && body && quoted &&
       json_quote(quoted,18000,d->minecraft) && json_quote(server,sizeof(server),server_id)) {
        snprintf(body,20000,"{\"accessToken\":%s,\"selectedProfile\":\"%s\",\"serverId\":%s}",quoted,d->id,server);
        ok=f->request(f->context,"https://sessionserver.mojang.com/session/minecraft/join","application/json",body,NULL,&r) && r.status==204;
        if(!ok)snprintf(f->error,sizeof(f->error),"Session join failed (HTTP %d). Check account/session and server authentication.",r.status);
    } else snprintf(f->error,sizeof(f->error),"Unable to prepare session join.");
    if(body){memset(body,0,20000);free(body);}if(quoted){memset(quoted,0,18000);free(quoted);}https_response_free(&r);return ok;
}
static int text(RecraftJson *j,int object,const char *key,char *out,size_t size)
{return json_string(j,json_member(j,object,key),out,size)&&out[0]!=0;}
static int error(AccountFlow *f,const char *message)
{snprintf(f->error,sizeof(f->error),"%s",message);return 0;}
static int services_rejection(AccountFlow *f,RecraftJson *j,long status)
{
    char message[256]={0};
    text(j,0,"errorMessage",message,sizeof(message));
    /* Show a bounded diagnosis, never dump a response containing credentials. */
    if(strstr(message,"app registration") || strstr(message,"App registration"))
        snprintf(f->error,sizeof(f->error),"Minecraft HTTP %ld: app registration rejected. Request Minecraft API access for this client_id.",status);
    else if(status==429 || status>=500)
        snprintf(f->error,sizeof(f->error),"Minecraft HTTP %ld: service unavailable. Retry sign-in later.",status);
    else if(status==200) error(f,"Minecraft returned an invalid access token response.");
    else snprintf(f->error,sizeof(f->error),"Minecraft HTTP %ld: sign-in rejected. Check app API access and Java account entitlement.",status);
    return 0;
}
static int texture_url(char *out,size_t capacity,const char *url)
{
    if(!strncmp(url,"http://textures.minecraft.net/",30)) snprintf(out,capacity,"https://%s",url+7);
    else if(!strncmp(url,"https://textures.minecraft.net/",31)) snprintf(out,capacity,"%s",url);
    else return 0;
    return 1;
}
static int microsoft_rejection(AccountFlow *f,RecraftJson *j)
{
    char kind[48]={0};int code=0;
    text(j,0,"error",kind,sizeof(kind));
    json_integer(j,json_element(j,json_member(j,0,"error_codes"),0),&code);
    if(code)snprintf(f->error,sizeof(f->error),"Microsoft: %s (AADSTS%d). Check app registration/public client flows.",kind,code);
    else error(f,"Microsoft rejected the device-code request. Check client_id and public client flows.");
    return 0;
}
static RecraftJson *request(AccountFlow *f,const char *url,const char *type,const char *body,const char *token,HttpsResponse *r)
{
    if(!f->request(f->context,url,type,body,token,r)) {error(f,"HTTPS request failed. Check connection and a modern TLS-capable curl helper.");return NULL;}
    return json_parse(r->data,r->size);
}
static int urlencode(char *out,size_t cap,const char *in)
{
    size_t at=0;const unsigned char *p=(const unsigned char *)in;static const char hex[]="0123456789ABCDEF";
    while(*p){unsigned c=*p++;int safe=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("-._~",(int)c);
        if(at+(safe?1:3)>=cap)return 0;
        if(safe)out[at++]=(char)c;else{out[at++]='%';out[at++]=hex[c>>4];out[at++]=hex[c&15];}}
    out[at]=0;return 1;
}
static int uuid32(const char *id)
{return strlen(id)==32&&strspn(id,"0123456789abcdefABCDEF")==32;}
int account_flow_run(AccountData *d,AccountFlow *f,int refresh)
{
    HttpsResponse r={0};RecraftJson *j=NULL;char *body=NULL,*quoted=NULL,*encoded=NULL;
    char device[8192]={0},code[32]={0},uri[128]={0},oauth_error[64],other_uhs[64],ticket[8400];
    int interval=5,expires=900,elapsed=0,object,ok=0,token_received=0,retry_device=0;size_t cap=65536;double deadline=0;
    f->error[0]=0;
    if(!account_client_id_valid(d->client_id))return error(f,"Set your ReCraft client_id in config/microsoft_auth.json first.");
    body=(char *)calloc(cap,1);quoted=(char *)malloc(cap);encoded=(char *)malloc(cap);
    if(!body||!quoted||!encoded){error(f,"Not enough memory for sign-in.");goto done;}
    if(!refresh) {
        f->progress(f->context,"Requesting a Microsoft device code...","","");
        snprintf(body,cap,"client_id=%s&scope=XboxLive.SignIn%%20XboxLive.offline_access",d->client_id);
        j=request(f,"https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode","application/x-www-form-urlencoded",body,NULL,&r);
        if(!j){if(!f->error[0])error(f,"Invalid Microsoft device-code response.");goto done;}
        if(r.status!=200){microsoft_rejection(f,j);goto done;}
        if(!text(j,0,"device_code",device,sizeof(device))||!text(j,0,"user_code",code,sizeof(code))||!text(j,0,"verification_uri",uri,sizeof(uri))) {error(f,"Invalid or oversized Microsoft device-code response.");goto done;}
        if(strcmp(uri,"https://www.microsoft.com/link")&&strcmp(uri,"https://microsoft.com/devicelogin")&&strcmp(uri,"https://www.microsoft.com/devicelogin")) {error(f,"Microsoft returned an unexpected sign-in address.");goto done;}
        json_integer(j,json_member(j,0,"interval"),&interval);json_integer(j,json_member(j,0,"expires_in"),&expires);
        if(interval<1||interval>30||expires<30||expires>1800){error(f,"Invalid device-code lifetime.");goto done;}
        if(f->now)deadline=f->now(f->context)+expires;
        f->progress(f->context,"Finish sign-in in your browser.",code,uri);json_free(j);j=NULL;https_response_free(&r);
    }
    while(refresh||(elapsed<expires&&(!f->now||f->now(f->context)<deadline))) {
        if(!refresh && !f->wait(f->context,(unsigned)interval)){error(f,"Sign-in cancelled.");goto done;}
        if(!urlencode(encoded,cap,refresh?d->refresh:device))goto done;
        snprintf(body,cap,"client_id=%s&grant_type=%s&%s=%s&scope=XboxLive.SignIn%%20XboxLive.offline_access",d->client_id,refresh?"refresh_token":"urn:ietf:params:oauth:grant-type:device_code",refresh?"refresh_token":"device_code",encoded);
        j=request(f,"https://login.microsoftonline.com/consumers/oauth2/v2.0/token","application/x-www-form-urlencoded",body,NULL,&r);
        if(!j)goto done;
        if(r.status!=200) {
            if(!text(j,0,"error",oauth_error,sizeof(oauth_error))){error(f,"Invalid Microsoft token response.");goto done;}
            if(refresh&&!strcmp(oauth_error,"invalid_grant")){retry_device=1;goto done;}
            if(!refresh&&(!strcmp(oauth_error,"authorization_pending")||!strcmp(oauth_error,"slow_down"))) {
                elapsed+=interval;if(!strcmp(oauth_error,"slow_down"))interval+=5;
                json_free(j);j=NULL;https_response_free(&r);continue;
            }
            error(f,!strcmp(oauth_error,"authorization_declined")?"Sign-in was declined.":!strcmp(oauth_error,"expired_token")?"Device code expired. Start sign-in again.":"Microsoft token exchange failed. Sign in again.");goto done;
        }
        if(!text(j,0,"access_token",d->msa,sizeof(d->msa))||!text(j,0,"refresh_token",d->refresh,sizeof(d->refresh))||!json_integer(j,json_member(j,0,"expires_in"),&expires)||expires<=0||expires>86400) {error(f,"Invalid Microsoft access token.");goto done;}
        d->issued=(int64_t)time(NULL);d->msa_exp=d->issued+expires;token_received=1;break;
    }
    if(!token_received) {error(f,"Device code expired. Start sign-in again.");goto done;}
    json_free(j);j=NULL;https_response_free(&r);
    f->progress(f->context,"Signing in to Xbox Live...","","");
    snprintf(ticket,sizeof(ticket),"d=%s",d->msa);if(!json_quote(quoted,cap,ticket))goto done;
    snprintf(body,cap,"{\"Properties\":{\"AuthMethod\":\"RPS\",\"SiteName\":\"user.auth.xboxlive.com\",\"RpsTicket\":%s},\"RelyingParty\":\"http://auth.xboxlive.com\",\"TokenType\":\"JWT\"}",quoted);
    j=request(f,"https://user.auth.xboxlive.com/user/authenticate","application/json",body,NULL,&r);
    object=json_element(j,json_member(j,json_member(j,0,"DisplayClaims"),"xui"),0);
    if(!j||r.status!=200||!text(j,0,"Token",d->xbox,sizeof(d->xbox))||!text(j,object,"uhs",d->uhs,sizeof(d->uhs))) {error(f,"Xbox Live sign-in failed. Verify that the account has an Xbox profile.");goto done;}
    json_free(j);j=NULL;https_response_free(&r);
    if(!json_quote(quoted,cap,d->xbox))goto done;
    snprintf(body,cap,"{\"Properties\":{\"SandboxId\":\"RETAIL\",\"UserTokens\":[%s]},\"RelyingParty\":\"rp://api.minecraftservices.com/\",\"TokenType\":\"JWT\"}",quoted);
    j=request(f,"https://xsts.auth.xboxlive.com/xsts/authorize","application/json",body,NULL,&r);
    object=json_element(j,json_member(j,json_member(j,0,"DisplayClaims"),"xui"),0);
    if(!j||r.status!=200||!text(j,0,"Token",d->xsts,sizeof(d->xsts))||!text(j,object,"uhs",other_uhs,sizeof(other_uhs))||strcmp(other_uhs,d->uhs)) {error(f,"Xbox authorization failed. Check Xbox profile, family restrictions and app access.");goto done;}
    json_free(j);j=NULL;https_response_free(&r);
    f->progress(f->context,"Fetching the Minecraft profile...","","");
    snprintf(ticket,sizeof(ticket),"XBL3.0 x=%s;%s",d->uhs,d->xsts);if(!json_quote(quoted,cap,ticket))goto done;
    snprintf(body,cap,"{\"xtoken\":%s,\"platform\":\"PC_LAUNCHER\"}",quoted);
    j=request(f,"https://api.minecraftservices.com/launcher/login","application/json",body,NULL,&r);
    if(!j||r.status!=200||!text(j,0,"access_token",d->minecraft,sizeof(d->minecraft))||!json_integer(j,json_member(j,0,"expires_in"),&expires)||expires<=0||expires>172800) {if(r.status)services_rejection(f,j,r.status);goto done;}
    d->minecraft_exp=(int64_t)time(NULL)+expires;json_free(j);j=NULL;https_response_free(&r);
    j=request(f,"https://api.minecraftservices.com/minecraft/profile",NULL,NULL,d->minecraft,&r);
    if(!j||r.status!=200||!text(j,0,"name",d->name,sizeof(d->name))||!settings_player_name_valid(d->name)||!text(j,0,"id",d->id,sizeof(d->id))||!uuid32(d->id)) {error(f,"No playable Minecraft Java profile. Check game ownership and profile creation.");goto done;}
    d->skin_url[0]=d->skin_id[0]=0;strcpy(d->skin_variant,"CLASSIC");
    object=json_member(j,0,"skins");
    {int n;for(n=0;n<16;++n){int skin=json_element(j,object,n);char state[24],url[512];if(skin<0)break;
        if(text(j,skin,"state",state,sizeof(state))&&!strcmp(state,"ACTIVE")&&text(j,skin,"url",url,sizeof(url))) {
            texture_url(d->skin_url,sizeof(d->skin_url),url);
            text(j,skin,"id",d->skin_id,sizeof(d->skin_id));text(j,skin,"variant",d->skin_variant,sizeof(d->skin_variant));break;
        }
    }}
    d->cape_id[0]=d->cape_url[0]=0;
    object=json_member(j,0,"capes");
    {int n;for(n=0;n<32;++n){int cape=json_element(j,object,n);char state[24],url[512];if(cape<0)break;
        if(text(j,cape,"state",state,sizeof(state))&&!strcmp(state,"ACTIVE")&&text(j,cape,"url",url,sizeof(url)) && texture_url(d->cape_url,sizeof(d->cape_url),url)) {
            text(j,cape,"id",d->cape_id,sizeof(d->cape_id));break;
        }
    }}
    ok=1;
done:
    json_free(j);https_response_free(&r);
    if(!ok&&!f->error[0])error(f,"Invalid authentication response.");
    if(body){memset(body,0,cap);free(body);}if(quoted){memset(quoted,0,cap);free(quoted);}if(encoded){memset(encoded,0,cap);free(encoded);}
    memset(ticket,0,sizeof(ticket));memset(device,0,sizeof(device));
    if(retry_device){memset(d->refresh,0,sizeof(d->refresh));return account_flow_run(d,f,0);}
    return ok;
}
