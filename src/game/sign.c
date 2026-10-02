#include "sign.h"
#include "../world/block_entity.h"
#include <math.h>
#include <string.h>

static int chunk_coordinate(int n)
{ return n>=0 ? n/16 : (int)(-((-(int64_t)n+15)/16)); }
static uint32_t read_utf8(const uint8_t *text,size_t size,size_t *offset,int modified)
{
    uint32_t u; unsigned extra,j; uint8_t b;
    if(*offset>=size) return 0;
    b=text[(*offset)++];
    if(b<128) return b;
    if(b>=0xc0 && b<=0xdf) { u=b&31; extra=1; }
    else if(b>=0xe0 && b<=0xef) { u=b&15; extra=2; }
    else if(!modified && b>=0xf0 && b<=0xf4) { u=b&7; extra=3; }
    else return 0xfffd;
    for(j=0;j<extra;++j) {
        if(*offset>=size || (text[*offset]&0xc0)!=0x80) return 0xfffd;
        u=(u<<6)|(text[(*offset)++]&63);
    }
    if((extra==1 && u<128 && !(modified && u==0)) ||
       (extra==2 && u<2048) || (extra==3 && u<65536) || u>0x10ffff ||
       (!modified && u>=0xd800 && u<=0xdfff)) return 0xfffd;
    return u;
}
static size_t write_utf8(uint8_t *text,uint32_t u)
{
    if(u<128) { text[0]=(uint8_t)u; return 1; }
    if(u<2048) { text[0]=(uint8_t)(0xc0|(u>>6)); text[1]=(uint8_t)(0x80|(u&63)); return 2; }
    if(u<65536) {
        text[0]=(uint8_t)(0xe0|(u>>12)); text[1]=(uint8_t)(0x80|((u>>6)&63));
        text[2]=(uint8_t)(0x80|(u&63)); return 3;
    }
    text[0]=(uint8_t)(0xf0|(u>>18)); text[1]=(uint8_t)(0x80|((u>>12)&63));
    text[2]=(uint8_t)(0x80|((u>>6)&63)); text[3]=(uint8_t)(0x80|(u&63)); return 4;
}
static int copy_line(char output[SIGN_LINE_BYTES],const uint8_t *input,size_t size,int modified)
{
    size_t read=0,written=0; int units=0;
    if(!input) { output[0]=0; return 0; }
    while(read<size && units<SIGN_LINE_UNITS) {
        uint32_t u=read_utf8(input,size,&read,modified); int n;
        if(modified && u>=0xd800 && u<=0xdbff) {
            size_t next=read; uint32_t low=read_utf8(input,size,&next,1);
            if(low>=0xdc00 && low<=0xdfff) { u=0x10000+((u-0xd800)<<10)+(low-0xdc00); read=next; }
            else u=0xfffd;
        } else if(u>=0xd800 && u<=0xdfff) u=0xfffd;
        if(u<32 || u==127) u=' ';
        n=u>=65536 ? 2 : 1;
        if(units+n>SIGN_LINE_UNITS) break;
        written+=write_utf8((uint8_t *)output+written,u); units+=n;
    }
    output[written]=0; return units;
}
int sign_line_copy(char output[SIGN_LINE_BYTES],const char *input)
{ return copy_line(output,(const uint8_t *)input,input ? strlen(input) : 0,0); }
void sign_line_read_nbt(char output[SIGN_LINE_BYTES],const uint8_t *input,size_t size)
{ (void)copy_line(output,input,size,1); }
size_t sign_line_write_nbt(uint8_t *output,size_t capacity,const char *input)
{
    char line[SIGN_LINE_BYTES]; size_t offset=0,size,written=0;
    sign_line_copy(line,input); size=strlen(line);
    while(offset<size) {
        uint32_t u=read_utf8((const uint8_t *)line,size,&offset,0);
        uint8_t bytes[6]; size_t n;
        if(u>=65536) {
            u-=0x10000; n=write_utf8(bytes,0xd800|(u>>10));
            n+=write_utf8(bytes+n,0xdc00|(u&1023));
        } else n=write_utf8(bytes,u);
        if(written+n>capacity) return 0;
        memcpy(output+written,bytes,n); written+=n;
    }
    return written;
}
int sign_is_block(unsigned id) { return id==63 || id==68; }
int sign_place(World *w,int x,int y,int z,unsigned face,float beta_yaw,int *px,int *py,int *pz)
{
    static const int dx[6]={0,0,0,0,-1,1},dy[6]={-1,1,0,0,0,0},dz[6]={0,0,-1,1,0,0};
    unsigned old,old_meta,meta; BlockEntity *entity;
    if(!w || w->network_mode || face<1 || face>5 || !isfinite(beta_yaw) ||
       !beta_material_solid(world_get_block(w,x,y,z))) return 0;
    x+=dx[face]; y+=dy[face]; z+=dz[face];
    if((unsigned)y>=WORLD_HEIGHT) return 0;
    old=world_get_block(w,x,y,z);
    old_meta=world_get_metadata(w,x,y,z);
    /* Block.canPlaceBlockAt accepts liquids and snow ground cover in Beta. */
    if(old!=0 && old!=8 && old!=9 && old!=10 && old!=11 && old!=78) return 0;
    meta=face==1 ? (unsigned)((int)floorf((fmodf(beta_yaw,360.0f)+180.0f)*16.0f/360.0f+.5f)&15) : face;
    if(!world_set_block(w,x,y,z,(uint8_t)(face==1 ? 63 : 68))) return 0;
    world_set_metadata(w,x,y,z,(uint8_t)meta);
    entity=block_entity_get(w,x,y,z,1);
    if(!entity) {
        world_set_block(w,x,y,z,(uint8_t)old);
        world_set_metadata(w,x,y,z,(uint8_t)old_meta); return 0;
    }
    if(px) *px=x;
    if(py) *py=y;
    if(pz) *pz=z;
    return 1;
}
void sign_neighbor_tick(World *w,int x,int y,int z)
{
    unsigned id,meta; int sx=x,sy=y,sz=z; Chunk *chunk;
    if(!w || w->network_mode) return;
    id=world_peek_block(w,x,y,z); meta=world_peek_metadata(w,x,y,z);
    if(id==63) --sy;
    else if(id==68) {
        if(meta==2) ++sz;
        else if(meta==3) --sz;
        else if(meta==4) ++sx;
        else if(meta==5) --sx;
        else { /* Invalid imported metadata has no supported wall attachment. */ sy=-1; }
    } else return;
    chunk=world_peek_chunk(w,chunk_coordinate(sx),chunk_coordinate(sz));
    if(!chunk || (w->beta_format && !chunk->beta_raw)) return;
    if(beta_material_solid(world_peek_block(w,sx,sy,sz))) return;
    if(!w->creative) world_drop_stack(w,x,y,z,(InventorySlot){323,1,0});
    world_set_block(w,x,y,z,0);
}
const char (*sign_text_get(const World *w,int x,int y,int z))[SIGN_LINE_BYTES]
{
    Chunk *chunk; BlockEntity *e;
    if(!w || !sign_is_block(world_peek_block(w,x,y,z))) return NULL;
    chunk=world_peek_chunk(w,chunk_coordinate(x),chunk_coordinate(z));
    if(!chunk) return NULL;
    for(e=chunk->entities;e;e=e->next)
        if(e->kind==BLOCK_ENTITY_SIGN && e->x==x && e->y==y && e->z==z) return e->sign_text;
    return NULL;
}
static int set_text(World *w,int x,int y,int z,const char lines[SIGN_LINES][SIGN_LINE_BYTES])
{
    BlockEntity *e; char text[SIGN_LINES][SIGN_LINE_BYTES]; int i;
    if(!w || !lines || !sign_is_block(world_peek_block(w,x,y,z))) return 0;
    e=block_entity_get(w,x,y,z,1); if(!e || e->kind!=BLOCK_ENTITY_SIGN) return 0;
    memset(text,0,sizeof(text));
    for(i=0;i<SIGN_LINES;++i) {
        size_t n=0;
        while(n<SIGN_LINE_BYTES && lines[i][n]) ++n;
        copy_line(text[i],(const uint8_t *)lines[i],n,0);
    }
    if(memcmp(text,e->sign_text,sizeof(text))) {
        memcpy(e->sign_text,text,sizeof(text)); e->sign_text_modified=1;
        if(!w->network_mode) block_entity_changed(w,e);
    }
    return 1;
}
int sign_text_set(World *w,int x,int y,int z,const char lines[SIGN_LINES][SIGN_LINE_BYTES])
{ return w && !w->network_mode ? set_text(w,x,y,z,lines) : 0; }
int sign_text_receive(World *w,int x,int y,int z,const char lines[SIGN_LINES][SIGN_LINE_BYTES])
{ return w && w->network_mode ? set_text(w,x,y,z,lines) : 0; }
