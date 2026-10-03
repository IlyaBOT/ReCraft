#include "json.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
/* Token spans borrow the response; at most 2048 tokens / 24 nesting levels. */
typedef struct Token { size_t start,end; int next,kind; } Token;
struct RecraftJson { const unsigned char *p; size_t n,at; Token token[2048]; int count; };
typedef struct RecraftJson Parser;
static void whitespace(Parser *p)
{ while(p->at<p->n && (p->p[p->at]==' '||p->p[p->at]=='\t'||p->p[p->at]=='\r'||p->p[p->at]=='\n')) ++p->at; }
static int hex(unsigned char c)
{ return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
static int unicode4(const unsigned char *p,unsigned *value)
{ unsigned v=0; int i,h; for(i=0;i<4;++i) { h=hex(p[i]); if(h<0) return 0; v=(v<<4)|(unsigned)h; } *value=v; return 1; }
static int utf8(const unsigned char *s,size_t n,size_t *at,unsigned *value)
{
    unsigned u,k,j,c; if(*at>=n) return 0; c=s[(*at)++];
    if(c<128) { *value=c; return 1; }
    if(c>=0xc2&&c<=0xdf) {u=c&31;k=1;} else if(c>=0xe0&&c<=0xef) {u=c&15;k=2;}
    else if(c>=0xf0&&c<=0xf4) {u=c&7;k=3;} else return 0;
    for(j=0;j<k;++j) {if(*at>=n) return 0;c=s[(*at)++];if((c&0xc0)!=0x80)return 0;u=(u<<6)|(c&63);}
    if((k==1&&u<128)||(k==2&&u<2048)||(k==3&&u<65536)||u>0x10ffff||(u>=0xd800&&u<=0xdfff))return 0;
    *value=u;return 1;
}
static int string_token(Parser *p,int id)
{
    ++p->at;p->token[id].start=p->at;
    while(p->at<p->n) {
        unsigned u,c=p->p[p->at++]; if(c=='"') {p->token[id].end=p->at-1;return 1;}
        if(c<32) return 0;
        if(c=='\\') {
            if(p->at>=p->n)return 0;
            c=p->p[p->at++];
            if(c=='u') {
                if(p->n-p->at<4||!unicode4(p->p+p->at,&u))return 0;
                p->at+=4;
                if(u>=0xd800&&u<=0xdbff) {unsigned lo;
                    if(p->n-p->at<6||p->p[p->at]!='\\'||p->p[p->at+1]!='u'||!unicode4(p->p+p->at+2,&lo)||lo<0xdc00||lo>0xdfff)return 0;
                    p->at+=6;
                } else if(u>=0xdc00&&u<=0xdfff)return 0;
            } else if(!strchr("\"\\/bfnrt",(int)c))return 0;
        } else if(c>=128) {size_t at=p->at-1;if(!utf8(p->p,p->n,&at,&u))return 0;p->at=at;}
    }return 0;
}
static int value(Parser *p,int depth)
{
    int id,kind; size_t start; unsigned char c;
    whitespace(p);if(p->at>=p->n||depth>24||p->count>=2048)return -1;
    id=p->count++;start=p->at;c=p->p[p->at];p->token[id].start=start;
    kind=c=='{'?1:c=='['?2:c=='"'?3:4;p->token[id].kind=kind;
    if(kind==3) {if(!string_token(p,id))return -1;}
    else if(kind==1||kind==2) {
        ++p->at;whitespace(p);
        if(p->at<p->n && p->p[p->at]==(kind==1?'}':']'))++p->at;
        else for(;;) {
            if(kind==1) {whitespace(p);if(p->at>=p->n||p->p[p->at]!='"'||value(p,depth+1)<0)return -1;
                whitespace(p);if(p->at>=p->n||p->p[p->at++]!=':')return -1;}
            if(value(p,depth+1)<0)return -1;
            whitespace(p);if(p->at>=p->n)return -1;
            c=p->p[p->at++];if(c==(kind==1?'}':']'))break;if(c!=',')return -1;
        }p->token[id].end=p->at;
    } else {
        if(c=='t'||c=='f'||c=='n') {const char *word=c=='t'?"true":c=='f'?"false":"null";size_t n=strlen(word);
            if(p->n-p->at<n||memcmp(p->p+p->at,word,n))return -1;
            p->at+=n;
        } else {
            if(c=='-')++p->at;
            if(p->at>=p->n)return -1;
            c=p->p[p->at];
            if(c=='0')++p->at;else {if(c<'1'||c>'9')return -1;while(p->at<p->n&&p->p[p->at]>='0'&&p->p[p->at]<='9')++p->at;}
            if(p->at<p->n&&p->p[p->at]=='.') {++p->at;start=p->at;while(p->at<p->n&&p->p[p->at]>='0'&&p->p[p->at]<='9')++p->at;if(p->at==start)return -1;}
            if(p->at<p->n&&(p->p[p->at]=='e'||p->p[p->at]=='E')) {++p->at;if(p->at<p->n&&(p->p[p->at]=='-'||p->p[p->at]=='+'))++p->at;
                start=p->at;while(p->at<p->n&&p->p[p->at]>='0'&&p->p[p->at]<='9')++p->at;if(p->at==start)return -1;}
        }p->token[id].end=p->at;
    }p->token[id].next=p->count;return id;
}
static void append_utf8(char *out,size_t cap,size_t *used,unsigned u)
{
    unsigned need=u<128?1:u<2048?2:u<65536?3:4;
    if(*used+need>=cap)return;
    if(need==1)out[(*used)++]=(char)u;else {
        if(need==4)out[(*used)++]=(char)(0xf0|(u>>18));
        if(need>=3)out[(*used)++]=(char)((need==3?0xe0:0x80)|((u>>12)&(need==3?15:63)));
        out[(*used)++]=(char)((need==2?0xc0:0x80)|((u>>6)&(need==2?31:63)));out[(*used)++]=(char)(0x80|(u&63));
    }
}
static int string_copy(Parser *p,int id,char *out,size_t cap,size_t *used)
{
    Token *t=p->token+id;size_t at=t->start;int truncated=0;
    if(t->kind!=3)return 0;
    while(at<t->end) {unsigned u;size_t before=*used;
        if(p->p[at]=='\\') {unsigned c=p->p[++at];++at;
            if(c=='u') {unicode4(p->p+at,&u);at+=4;if(u>=0xd800&&u<=0xdbff){unsigned lo;unicode4(p->p+at+2,&lo);at+=6;u=0x10000+((u-0xd800)<<10)+lo-0xdc00;}}
            else u=c=='n'?'\n':c=='t'?'\t':c=='r'?'\r':c=='b'?'\b':c=='f'?'\f':c;
        }else if(!utf8(p->p,t->end,&at,&u))return 0;
        /* Do not silently rewrite tokens, URLs or filenames from a response. */
        if(u<32||u==127)return 0;
        append_utf8(out,cap,used,u);if(*used==before)truncated=1;
    }out[*used]=0;return !truncated;
}
static int key_equals(Parser *p,int id,const char *key)
{ char text[48];size_t used=0;return string_copy(p,id,text,sizeof(text),&used)&&!strcmp(text,key); }
static int member(Parser *p,int id,const char *key)
{ int k;if(id<0||p->token[id].kind!=1)return -1;for(k=id+1;k<p->token[id].next;) {int v=k+1;if(key_equals(p,k,key))return v;k=p->token[v].next;}return -1; }
static int integer(Parser *p,int id,int *out)
{ size_t at;unsigned long v=0;if(id<0||p->token[id].kind!=4)return 0;for(at=p->token[id].start;at<p->token[id].end;++at){unsigned c=p->p[at];if(c<'0'||c>'9'||v>((unsigned long)INT_MAX-(c-'0'))/10)return 0;v=v*10+c-'0';}*out=(int)v;return at>p->token[id].start; }
RecraftJson *json_parse(const void *data,size_t size)
{
    Parser *p;int root;
    if(!data || !size || size>128*1024) return NULL;
    p=(Parser *)calloc(1,sizeof(*p));if(!p)return NULL;p->p=(const unsigned char *)data;p->n=size;
    root=value(p,0);whitespace(p);
    if(root!=0 || p->at!=size){free(p);return NULL;}return p;
}
void json_free(RecraftJson *json){free(json);}
int json_member(RecraftJson *json,int object,const char *key)
{ return json&&object<json->count?member(json,object,key):-1; }
int json_element(RecraftJson *json,int array,int index)
{
    int at;if(!json || array<0 || array>=json->count || json->token[array].kind!=2 || index<0)return -1;
    at=array+1;while(index-- && at<json->token[array].next)at=json->token[at].next;
    return at<json->token[array].next?at:-1;
}
int json_string(RecraftJson *json,int token,char *out,size_t capacity)
{ size_t used=0;if(!out||!capacity)return 0;out[0]=0;return json&&token>=0&&token<json->count&&string_copy(json,token,out,capacity,&used); }
int json_integer(RecraftJson *json,int token,int *out)
{return json&&token>=0&&token<json->count?integer(json,token,out):0;}
int json_true(RecraftJson *json,int token)
{return json&&token>=0&&token<json->count&&json->token[token].kind==4&&json->token[token].end-json->token[token].start==4&&!memcmp(json->p+json->token[token].start,"true",4);}
int json_quote(char *out,size_t capacity,const char *text)
{
    size_t at=0;const unsigned char *p=(const unsigned char *)text;
    if(!out||!p||capacity<3)return 0;
    out[at++]='"';
    while(*p) {
        unsigned c=*p++;
        if(c<32 || c==127) return 0;
        if(c=='"'||c=='\\'){if(at+3>=capacity)return 0;out[at++]='\\';}
        if(at+2>=capacity)return 0;
        out[at++]=(char)c;
    }
    out[at++]='"';out[at]=0;return 1;
}
