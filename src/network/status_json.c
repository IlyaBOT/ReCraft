#include "server_status.h"
#include "../assets/server_icon_png.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Token spans borrow the response; at most 2048 tokens / 24 nesting levels. */
typedef struct Token { size_t start,end; int next,kind; } Token;
typedef struct Parser { const unsigned char *p; size_t n,at; Token token[2048]; int count; } Parser;
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
    if(u==0||u==127||(u<32&&u!='\n'))u=' ';
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
            else u=c=='n'?'\n':c=='t'?' ':c=='r'?' ':c=='b'?' ':c=='f'?' ':c;
        }else if(!utf8(p->p,t->end,&at,&u))return 0;
        append_utf8(out,cap,used,u);if(*used==before)truncated=1;
    }out[*used]=0;return !truncated;
}
static int key_equals(Parser *p,int id,const char *key)
{ char text[48];size_t used=0;return string_copy(p,id,text,sizeof(text),&used)&&!strcmp(text,key); }
static int member(Parser *p,int id,const char *key)
{ int k;if(id<0||p->token[id].kind!=1)return -1;for(k=id+1;k<p->token[id].next;) {int v=k+1;if(key_equals(p,k,key))return v;k=p->token[v].next;}return -1; }
static int integer(Parser *p,int id,int *out)
{ size_t at;unsigned long v=0;if(id<0||p->token[id].kind!=4)return 0;for(at=p->token[id].start;at<p->token[id].end;++at){unsigned c=p->p[at];if(c<'0'||c>'9'||v>((unsigned long)INT_MAX-(c-'0'))/10)return 0;v=v*10+c-'0';}*out=(int)v;return at>p->token[id].start; }
static int protocol_integer(Parser *p,int id,int *out)
{
    if(id>=0&&p->token[id].kind==4&&p->token[id].end-p->token[id].start==2&&
       p->p[p->token[id].start]=='-'&&p->p[p->token[id].start+1]=='1'){*out=-1;return 1;}
    return integer(p,id,out);
}
static int color_code(Parser *p,int id)
{
    static const char *const names[]={"black","dark_blue","dark_green","dark_aqua","dark_red","dark_purple","gold","gray","dark_gray","blue","green","aqua","red","light_purple","yellow","white"};
    char text[32];size_t used=0;int i;if(id<0||!string_copy(p,id,text,sizeof(text),&used))return -1;
    for(i=0;i<16;++i)if(!strcmp(text,names[i]))return i;
    return !strcmp(text,"reset")?7:-1;
}
static void append_color(char *out,size_t cap,size_t *used,int code)
{
    static const char colors[]="0123456789abcdef";
    if(*used+3>=cap)return;
    out[(*used)++]=(char)0xc2;out[(*used)++]=(char)0xa7;out[(*used)++]=colors[code<0?7:code];out[*used]=0;
}
static void description(Parser *p,int id,char *out,size_t cap,size_t *used,int depth,int inherited)
{
    int k,t,color;if(id<0||depth>24)return;
    if(p->token[id].kind==3) {(void)string_copy(p,id,out,cap,used);return;}
    if(p->token[id].kind==2) {for(k=id+1;k<p->token[id].next;k=p->token[k].next)description(p,k,out,cap,used,depth+1,inherited);return;}
    if(p->token[id].kind!=1)return;
    color=color_code(p,member(p,id,"color"));if(color<0)color=inherited;
    if(color!=inherited)append_color(out,cap,used,color);
    t=member(p,id,"text");if(t<0)t=member(p,id,"translate");description(p,t,out,cap,used,depth+1,color);
    t=member(p,id,"extra");description(p,t,out,cap,used,depth+1,color);
    if(color!=inherited)append_color(out,cap,used,inherited);
}
static int base64_value(unsigned c)
{return c>='A'&&c<='Z'?(int)(c-'A'):c>='a'&&c<='z'?(int)(c-'a'+26):c>='0'&&c<='9'?(int)(c-'0'+52):c=='+'?62:c=='/'?63:-1;}
static void favicon(Parser *p,int id,ServerStatusResult *out)
{
    char data[SERVER_STATUS_ICON_MAX*4/3+64];size_t used=0,i,w=0,n;const char *b;unsigned char *png=out->icon;
    if(id<0||!string_copy(p,id,data,sizeof(data),&used)||strncmp(data,"data:image/png;base64,",22))return;
    b=data+22;n=used-22;if(!n||n%4)return;
    for(i=0;i<n;i+=4) {int a=base64_value((unsigned char)b[i]),c=base64_value((unsigned char)b[i+1]);
        int d=b[i+2]=='='?-2:base64_value((unsigned char)b[i+2]);int e=b[i+3]=='='?-2:base64_value((unsigned char)b[i+3]);unsigned bits;
        if(a<0||c<0||d==-1||e==-1||(d==-2&&e!=-2)||((d==-2||e==-2)&&i+4!=n))return;
        if((d==-2&&(c&15))||(e==-2&&d>=0&&(d&3)))return;
        bits=((unsigned)a<<18)|((unsigned)c<<12)|((unsigned)(d>=0?d:0)<<6)|(unsigned)(e>=0?e:0);
        if(w>=SERVER_STATUS_ICON_MAX)return;
        png[w++]=(unsigned char)(bits>>16);
        if(d>=0){if(w>=SERVER_STATUS_ICON_MAX)return;png[w++]=(unsigned char)(bits>>8);}
        if(e>=0){if(w>=SERVER_STATUS_ICON_MAX)return;png[w++]=(unsigned char)bits;}
    }
    if(w<33||memcmp(png,"\x89PNG\r\n\x1a\n",8)||memcmp(png+12,"IHDR",4)||
       png[8]||png[9]||png[10]||png[11]!=13||png[16]||png[17]||png[18]||png[19]!=64||
       png[20]||png[21]||png[22]||png[23]!=64||
       (png[24]!=1&&png[24]!=2&&png[24]!=4&&png[24]!=8&&png[24]!=16)||
       (png[25]!=0&&png[25]!=2&&png[25]!=3&&png[25]!=4&&png[25]!=6)||png[26]||png[27]||png[28]>1)return;
    if(server_icon_png_valid(png,w))out->icon_size=w;
}
int server_status_parse_json(const char *json,size_t length,ServerStatusResult *out)
{
    Parser *p;int root,v,players,id;size_t used=0;int result=0;
    if(!json||!out||!length||length>SERVER_STATUS_JSON_MAX)return 0;
    p=(Parser *)calloc(1,sizeof(*p));if(!p)return 0;p->p=(const unsigned char *)json;p->n=length;
    root=value(p,0);whitespace(p);if(root!=0||p->at!=length||p->token[root].kind!=1)goto done;
    v=member(p,root,"version");players=member(p,root,"players");
    if(v<0&&players<0&&member(p,root,"description")<0)goto done;
    out->protocol=0;out->online=out->maximum=-1;
    if(v>=0&&!protocol_integer(p,member(p,v,"protocol"),&out->protocol))goto done;
    if(players>=0&&(!integer(p,member(p,players,"online"),&out->online)||!integer(p,member(p,players,"max"),&out->maximum)))goto done;
    out->motd[0]=out->version[0]=0;out->icon_size=0;
    id=member(p,v,"name");if(id>=0)(void)string_copy(p,id,out->version,sizeof(out->version),&used);
    else strcpy(out->version,"Old");
    used=0;id=member(p,root,"description");description(p,id,out->motd,sizeof(out->motd),&used,0,7);out->motd[used]=0;
    favicon(p,member(p,root,"favicon"),out);result=1;
done:free(p);return result;
}
int server_status_ping_bars(int ms)
{return ms<0?-1:ms<150?5:ms<300?4:ms<600?3:ms<1000?2:1;}
