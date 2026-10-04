#include "language.h"
#include "../assets/assets.h"
#include "../game/creative.h"
#include "../world/beta_blocks.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct Translation { char *key,*value; } Translation;
typedef struct Table { char *bytes; Translation *items; unsigned count; } Table;
static Table english,selected;
static LanguageEntry languages[128];
static int count;
static void free_table(Table *t) { free(t->items); free(t->bytes); memset(t,0,sizeof(*t)); }
static int compare(const void *a,const void *b) { return strcmp(((const Translation *)a)->key,((const Translation *)b)->key); }
static int load(Table *out,const char *code)
{
    Table t={0}; char path[80],*line; size_t size; unsigned capacity=4096;
    snprintf(path,sizeof(path),"lang/%s.lang",code); t.bytes=(char *)assets_read_file(path,&size);
    if(!t.bytes || size>1024*1024) { free(t.bytes); return 0; }
    t.items=(Translation *)calloc(capacity,sizeof(*t.items)); if(!t.items) { free(t.bytes); return 0; }
    line=t.bytes;
    while(*line && t.count<capacity) {
        char *next=strchr(line,'\n'),*equal,*end;
        if(next) *next++=0;
        end=line+strlen(line); if(end>line && end[-1]=='\r') end[-1]=0;
        equal=strchr(line,'=');
        if(equal && equal!=line && line[0]!='#') { *equal=0; t.items[t.count++]=(Translation){line,equal+1}; }
        if(!next) break;
        line=next;
    }
    qsort(t.items,t.count,sizeof(*t.items),compare); free_table(out); *out=t; return 1;
}
static const char *find(const Table *t,const char *key)
{
    unsigned lo=0,hi=t->count;
    while(lo<hi) { unsigned m=(lo+hi)/2; int c=strcmp(key,t->items[m].key); if(c==0) return t->items[m].value; if(c<0) hi=m; else lo=m+1; }
    return NULL;
}
static int language_compare(const void *a,const void *b) { return strcmp(((const LanguageEntry *)a)->code,((const LanguageEntry *)b)->code); }
void language_init(void)
{
    char *bytes,*line; size_t size;
    language_shutdown(); load(&english,"en_US");
    strcpy(languages[0].code,"en_US"); strcpy(languages[0].name,"English (US)"); count=1;
    bytes=(char *)assets_read_file("lang/languages.txt",&size); if(!bytes) return;
    line=bytes;
    while(*line && count<128) {
        char *next=strchr(line,'\n'),*equal; if(next) *next++=0;
        line[strcspn(line,"\r")]=0; equal=strchr(line,'=');
        if(equal && equal-line>0 && equal-line<16) {
            *equal=0;
            if(strcmp(line,"en_US")) {
                snprintf(languages[count].code,sizeof(languages[count].code),"%s",line);
                snprintf(languages[count].name,sizeof(languages[count].name),"%s",equal+1); ++count;
            }
        }
        if(!next) break;
        line=next;
    }
    free(bytes); qsort(languages,(size_t)count,sizeof(*languages),language_compare);
}
void language_shutdown(void) { free_table(&english); free_table(&selected); count=0; }
int language_count(void) { return count; }
const LanguageEntry *language_at(int index) { return index>=0 && index<count ? languages+index : NULL; }
int language_select(const char *code)
{
    int i;
    for(i=0;i<count;++i) if(code && !strcmp(languages[i].code,code)) return load(&selected,code);
    return 0;
}
const char *language_text(const char *key,const char *fallback)
{
    const char *s=find(&selected,key); if(!s) s=find(&english,key); return s ? s : fallback;
}
const char *language_item(int id,int damage)
{
    const char *key=beta_item_translation_key(id,damage),*name=beta_item_variant_name(id,damage);
    if(!name)name=beta_item_name(id);
    if(!name){const BetaBlockDef *block=beta_block_find((unsigned)id);if(block)name=block->name;}
    if(!name)name="";
    if(id==2256 || id==2257) {
        static char record[128];snprintf(record,sizeof(record),"%s (%s)",language_text("item.record.name","Music Disc"),id==2256 ? "13" : "cat");return record;
    }
    return key ? language_text(key,name) : language_caption(name);
}
const char *language_caption(const char *caption)
{
    static char formatted[4][512]; static unsigned slot;
    unsigned i; const char *colon=strchr(caption,':'); size_t prefix=colon ? (size_t)(colon-caption) : 0;
    for(i=0;i<english.count;++i) {
        const Translation *e=english.items+i;
        if(!strcmp(e->value,caption)) return language_text(e->key,caption);
        if(prefix && strlen(e->value)==prefix && !strncmp(e->value,caption,prefix)) {
            char *out=formatted[slot++%4]; snprintf(out,512,"%s%s",language_text(e->key,e->value),colon); return out;
        }
    }
    return caption;
}
