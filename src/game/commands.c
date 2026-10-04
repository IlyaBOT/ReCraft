#include "commands.h"
#include "../world/environment.h"
#include "../world/transport.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int integer(const char *s,long min,long max,long *out)
{
    char *end; long v;
    if(!s || !*s) return 0;
    errno=0; v=strtol(s,&end,10);
    if(errno || *end || v<min || v>max) return 0;
    *out=v; return 1;
}
static int self(const Player *p,const char *s)
{ return p->name && !strcmp(p->name,s); }
static int coordinate(const char *s,double base,int horizontal,float *out)
{
    char *end; double v=0; int relative=s[0]=='~';
    if(relative) ++s;
    if(*s) { errno=0; v=strtod(s,&end); if(errno || *end || !isfinite(v)) return 0; }
    else if(!relative) return 0;
    if(relative) v+=base;
    else if(horizontal && !strchr(s,'.')) v+=.5;
    if(!isfinite(v) || (horizontal ? fabs(v)>30000000 : v < -4096 || v > 4096)) return 0;
    *out=(float)v; return 1;
}
int game_command(World *w,Player *p,const char *line,char *reply,size_t cap)
{
    char text[401],*args[9],*at; int n=0,i;
    long value;
    if(!reply || !cap) return 0;
    reply[0]=0;
    if(!w || !p || !line || line[0]!='/' || w->network_mode) return 0;
    if(strlen(line)>=sizeof(text)) goto invalid;
    strcpy(text,line+1); at=text;
    while(*at) {
        while(isspace((unsigned char)*at)) ++at;
        if(!*at) break;
        if(n==9) goto invalid;
        args[n++]=at;
        while(*at && !isspace((unsigned char)*at)) ++at;
        if(*at) *at++=0;
    }
    if(!n) goto invalid;
    if(!strcmp(args[0],"help") && n==1) {
        snprintf(reply,cap,"/tp [name] x y z; /gamemode survival|creative; /time set|add; /timeset; /weather; /seed");
        return 1;
    }
    if(!strcmp(args[0],"tp")) {
        float x,y,z; i=n==5 ? 2 : 1;
        if((n!=4 && n!=5) || (n==5 && !self(p,args[1])) ||
           !coordinate(args[i],p->x,1,&x) || !coordinate(args[i+1],p->y,0,&y) ||
           !coordinate(args[i+2],p->z,1,&z)) goto invalid;
        if(p->riding) world_minecart_dismount(w,p);
        p->sleeping=0; p->sleep_ticks=0;
        p->x=x; p->y=y; p->z=z; p->vx=p->vy=p->vz=0;
        p->push_x=p->push_z=0; p->fall_distance=0; p->on_ground=0;
        snprintf(reply,cap,"Teleported to %.2f %.2f %.2f",x,y,z); return 1;
    }
    if(!strcmp(args[0],"gamemode")) {
        int creative;
        if((n!=2 && n!=3) || (n==3 && !self(p,args[2]))) goto invalid;
        if(!strcmp(args[1],"creative") || !strcmp(args[1],"c") || !strcmp(args[1],"1")) creative=1;
        else if(!strcmp(args[1],"survival") || !strcmp(args[1],"s") || !strcmp(args[1],"0")) creative=0;
        else goto invalid;
        p->creative=w->creative=(uint8_t)creative; p->fall_distance=0;
        if(!creative) p->flying=0;
        snprintf(reply,cap,"Game mode: %s",creative ? "creative" : "survival"); return 1;
    }
    if(!strcmp(args[0],"time") || !strcmp(args[0],"timeset")) {
        int alias=!strcmp(args[0],"timeset"),add=0; const char *s;
        if(alias) { if(n!=2) goto invalid; s=args[1]; }
        else {
            if(n!=3 || (strcmp(args[1],"set") && strcmp(args[1],"add"))) goto invalid;
            add=!strcmp(args[1],"add"); s=args[2];
        }
        if(!add && !strcmp(s,"day")) value=0;
        else if(!add && !strcmp(s,"night")) value=12500;
        else if(!integer(s,0,INT_MAX,&value)) goto invalid;
        if(add && w->beta_world_time>INT64_MAX-value) goto invalid;
        w->beta_world_time=add ? w->beta_world_time+value : value;
        world_environment_refresh(w);
        snprintf(reply,cap,"Time: %lld",(long long)w->beta_world_time); return 1;
    }
    if(!strcmp(args[0],"weather")) {
        int rain,thunder; long duration=300;
        if(n!=2 && n!=3) goto invalid;
        rain=strcmp(args[1],"clear")!=0; thunder=!strcmp(args[1],"thunder");
        if(rain && !thunder && strcmp(args[1],"rain")) goto invalid;
        if(n==3 && !integer(args[2],1,1000000,&duration)) goto invalid;
        w->raining=rain; w->thundering=thunder;
        w->rain_time=w->thunder_time=(int)duration*20;
        world_environment_refresh(w);
        snprintf(reply,cap,"Weather: %s",args[1]); return 1;
    }
    if(!strcmp(args[0],"seed") && n==1) {
        int64_t seed; memcpy(&seed,&w->seed,sizeof(seed));
        snprintf(reply,cap,"Seed: %lld",(long long)seed); return 1;
    }
invalid:
    snprintf(reply,cap,"Invalid command or arguments. Use /help."); return -1;
}
