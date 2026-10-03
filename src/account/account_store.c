#include "account_flow.h"
#include "../util/json.h"
#include "../util/file_dialog.h"
#include "../game/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#define store_pid() _getpid()
#else
#include <unistd.h>
#include <fcntl.h>
#define store_pid() getpid()
#endif
static int text(RecraftJson *j,int parent,const char *key,char *out,size_t cap)
{return json_string(j,json_member(j,parent,key),out,cap);}
int account_data_load(AccountData *d,const char *path)
{
    size_t size;char *bytes=(char *)game_read_small_file(path,128*1024,&size);RecraftJson *j;AccountData *loaded;
    int version,accounts,selected=-1,n,object,ok=0;char type[16];
    if(!bytes)return 0;
    j=json_parse(bytes,size);loaded=(AccountData *)calloc(1,sizeof(*loaded));if(!j||!loaded)goto done;
    if(!json_integer(j,json_member(j,0,"formatVersion"),&version)||version!=3)goto done;
    accounts=json_member(j,0,"accounts");
    for(n=0;n<32;++n){int a=json_element(j,accounts,n);if(a<0)break;if(text(j,a,"type",type,sizeof(type))&&!strcmp(type,"MSA")) {
        if(selected<0)selected=a;
        if(json_true(j,json_member(j,a,"active"))){selected=a;break;}}}
    if(selected<0||!text(j,selected,"msa-client-id",loaded->client_id,sizeof(loaded->client_id))||!account_client_id_valid(loaded->client_id))goto done;
    object=json_member(j,selected,"msa");
    text(j,object,"token",loaded->msa,sizeof(loaded->msa));text(j,object,"refresh_token",loaded->refresh,sizeof(loaded->refresh));
    {int number;if(json_integer(j,json_member(j,object,"iat"),&number))loaded->issued=number;if(json_integer(j,json_member(j,object,"exp"),&number))loaded->msa_exp=number;}
    object=json_member(j,selected,"ygg");text(j,object,"token",loaded->minecraft,sizeof(loaded->minecraft));
    {int number;if(json_integer(j,json_member(j,object,"exp"),&number))loaded->minecraft_exp=number;}
    text(j,json_member(j,selected,"utoken"),"token",loaded->xbox,sizeof(loaded->xbox));
    object=json_member(j,selected,"xrp-mc");text(j,object,"token",loaded->xsts,sizeof(loaded->xsts));text(j,json_member(j,object,"extra"),"uhs",loaded->uhs,sizeof(loaded->uhs));
    object=json_member(j,selected,"profile");
    if(!text(j,object,"name",loaded->name,sizeof(loaded->name))||!settings_player_name_valid(loaded->name)||!text(j,object,"id",loaded->id,sizeof(loaded->id))||strlen(loaded->id)!=32||strspn(loaded->id,"0123456789abcdefABCDEF")!=32||!loaded->refresh[0])goto done;
    object=json_member(j,object,"skin");text(j,object,"id",loaded->skin_id,sizeof(loaded->skin_id));text(j,object,"url",loaded->skin_url,sizeof(loaded->skin_url));text(j,object,"variant",loaded->skin_variant,sizeof(loaded->skin_variant));
    *d=*loaded;ok=1;
done:
    json_free(j);if(loaded){memset(loaded,0,sizeof(*loaded));free(loaded);}memset(bytes,0,size);free(bytes);return ok;
}
static FILE *private_file(const char *path)
{
#ifdef _WIN32
    HANDLE token=NULL,file=INVALID_HANDLE_VALUE;DWORD needed=0;TOKEN_USER *user=NULL;PACL acl=NULL;EXPLICIT_ACCESSA access;SECURITY_DESCRIPTOR descriptor;SECURITY_ATTRIBUTES security;
    FILE *result=NULL;int fd;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))goto done;
    GetTokenInformation(token,TokenUser,NULL,0,&needed);user=(TOKEN_USER *)malloc(needed);if(!user||!GetTokenInformation(token,TokenUser,user,needed,&needed))goto done;
    memset(&access,0,sizeof(access));access.grfAccessPermissions=GENERIC_ALL;access.grfAccessMode=SET_ACCESS;
    access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.TrusteeType=TRUSTEE_IS_USER;access.Trustee.ptstrName=(LPSTR)user->User.Sid;
    if(SetEntriesInAclA(1,&access,NULL,&acl)!=ERROR_SUCCESS||!InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION)||!SetSecurityDescriptorDacl(&descriptor,TRUE,acl,FALSE)||!SetSecurityDescriptorControl(&descriptor,SE_DACL_PROTECTED,SE_DACL_PROTECTED))goto done;
    security.nLength=sizeof(security);security.lpSecurityDescriptor=&descriptor;security.bInheritHandle=FALSE;
    file=CreateFileA(path,GENERIC_WRITE,0,&security,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)goto done;
    fd=_open_osfhandle((intptr_t)file,_O_WRONLY|_O_BINARY);if(fd<0)goto done;file=INVALID_HANDLE_VALUE;
    result=_fdopen(fd,"wb");if(!result)_close(fd);
done:
    if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    if(token)CloseHandle(token);
    if(acl)LocalFree(acl);
    free(user);return result;
#else
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);FILE *result;
    if(fd<0)return NULL;
    result=fdopen(fd,"wb");if(!result)close(fd);return result;
#endif
}
static int string(FILE *f,const char *key,const char *value,char *quote,size_t cap)
{return json_quote(quote,cap,value)&&fprintf(f,"\"%s\":%s",key,quote)>0;}
static int token(FILE *f,const char *key,const char *value,const char *refresh,int64_t issued,int64_t expiry,const char *uhs,char *q,size_t cap)
{
    if(fprintf(f,"\"%s\":{\"iat\":%lld,\"exp\":%lld,",key,(long long)issued,(long long)expiry)<0||!string(f,"token",value,q,cap))return 0;
    if(refresh&&*refresh && (fputc(',',f)==EOF||!string(f,"refresh_token",refresh,q,cap)))return 0;
    if(uhs&&*uhs && (fputs(",\"extra\":{",f)<0||!string(f,"uhs",uhs,q,cap)||fputc('}',f)==EOF))return 0;
    return fputc('}',f)!=EOF;
}
int account_data_save(const AccountData *d,const char *path)
{
    char temp[1024],*q=(char *)malloc(32768);FILE *f;int ok=0;
    if(!q||strlen(path)>950||!account_client_id_valid(d->client_id)||!settings_player_name_valid(d->name)||strlen(d->id)!=32||strspn(d->id,"0123456789abcdefABCDEF")!=32){free(q);return 0;}
    snprintf(temp,sizeof(temp),"%s.tmp-%ld",path,(long)store_pid());f=private_file(temp);if(!f){free(q);return 0;}
    if(fputs("{\"formatVersion\":3,\"accounts\":[{\"active\":true,\"type\":\"MSA\",",f)<0||!string(f,"msa-client-id",d->client_id,q,32768)||fputc(',',f)==EOF||
       !token(f,"msa",d->msa,d->refresh,d->issued,d->msa_exp,NULL,q,32768)||fputc(',',f)==EOF||
       !token(f,"utoken",d->xbox,NULL,d->issued,d->msa_exp,d->uhs,q,32768)||fputc(',',f)==EOF||
       !token(f,"xrp-mc",d->xsts,NULL,d->issued,d->msa_exp,d->uhs,q,32768)||fputc(',',f)==EOF||
       !token(f,"ygg",d->minecraft,NULL,d->issued,d->minecraft_exp,NULL,q,32768)||fputs(",\"profile\":{",f)<0||
       !string(f,"id",d->id,q,32768)||fputc(',',f)==EOF||!string(f,"name",d->name,q,32768)||fputs(",\"skin\":{",f)<0||
       !string(f,"id",d->skin_id,q,32768)||fputc(',',f)==EOF||!string(f,"url",d->skin_url,q,32768)||fputc(',',f)==EOF||
       !string(f,"variant",d->skin_variant,q,32768)||fputs("},\"capes\":[]}}]}\n",f)<0||fflush(f))goto done;
#ifdef _WIN32
    if(_commit(_fileno(f)))goto done;
#else
    if(fsync(fileno(f)))goto done;
#endif
    ok=1;
done:
    if(fclose(f))ok=0;
#ifdef _WIN32
    if(ok&&!MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))ok=0;
#else
    if(ok&&rename(temp,path))ok=0;
#endif
    if(!ok)remove(temp);
    memset(q,0,32768);free(q);return ok;
}
