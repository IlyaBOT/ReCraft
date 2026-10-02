#include "game/sign.h"
#include "game/modal_input.h"
#include "world/block_entity.h"
#include "nbt/nbt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_mkdir(p) _mkdir(p)
#define test_rmdir(p) _rmdir(p)
#define test_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir(p,0700)
#define test_rmdir(p) rmdir(p)
#define test_pid() getpid()
#endif

static void text_tests(void)
{
    char line[SIGN_LINE_BYTES],decoded[SIGN_LINE_BYTES]; uint8_t modified[91]; size_t n;
    const char unicode[]="A\xd0\x9f\xf0\x9f\x98\x80"; /* ASCII, Cyrillic, supplementary code point. */
    const uint8_t java[]={'A',0xd0,0x9f,0xed,0xa0,0xbd,0xed,0xb8,0x80};
    assert(sign_line_copy(line,"1234567890123456789")==15 && !strcmp(line,"123456789012345"));
    assert(sign_line_copy(line,unicode)==4 && !strcmp(line,unicode));
    n=sign_line_write_nbt(modified,sizeof(modified),line);
    assert(n==sizeof(java) && !memcmp(modified,java,n));
    sign_line_read_nbt(decoded,modified,n); assert(!strcmp(decoded,unicode));
    assert(sign_line_copy(line,"12345678901234\xf0\x9f\x98\x80")==14 && !strcmp(line,"12345678901234"));
    assert(sign_line_copy(line,"one\ntwo\t")==8 && !strcmp(line,"one two "));
    assert(sign_line_copy(line,"\xff")==1 && !strcmp(line,"\xef\xbf\xbd"));
    assert(sign_line_copy(line,NULL)==0 && !line[0]);
    sign_line_read_nbt(line,(const uint8_t *)"1234567890123456789",19);
    assert(!strcmp(line,"123456789012345"));
}
static void placement_tests(void)
{
    static const int dx[6]={0,0,0,0,-1,1},dz[6]={0,0,-1,1,0,0};
    World w; int i,x,y,z; BetaBlockBox box; WorldDropEvent drop;
    char lines[SIGN_LINES][SIGN_LINE_BYTES]={{0}};
    assert(world_init(&w,42,1,16)==WORLD_OK);
    assert(!sign_place(&w,4,63,4,0,0,NULL,NULL,NULL));
    assert(!sign_place(&w,4,64,4,1,0,NULL,NULL,NULL));
    for(i=0;i<16;++i) {
        assert(sign_place(&w,4,63,4,1,i*22.5f-180.0f,&x,&y,&z));
        assert(x==4 && y==64 && z==4 && world_peek_block(&w,x,y,z)==63);
        assert(world_peek_metadata(&w,x,y,z)==i);
        assert(block_entity_get(&w,x,y,z,0)->kind==BLOCK_ENTITY_SIGN);
        assert(sign_text_get(&w,x,y,z) && !sign_text_get(&w,x,y,z)[0][0]);
        assert(world_set_block(&w,x,y,z,0) && !block_entity_get(&w,x,y,z,0));
    }
    assert(world_set_block(&w,4,63,4,20)); /* Transparent material remains solid support. */
    assert(sign_place(&w,4,63,4,1,-540.0f,&x,&y,&z) && world_peek_metadata(&w,x,y,z)==0);
    strcpy(lines[0],"Read this sign"); assert(sign_text_set(&w,x,y,z,lines));
    assert(!strcmp(sign_text_get(&w,x,y,z)[0],lines[0]));
    assert(world_set_block(&w,4,63,4,0)); world_step_physics(&w,4096);
    assert(!world_peek_block(&w,4,64,4) && !sign_text_get(&w,4,64,4));
    assert(world_take_drop(&w,&drop) && drop.id==323 && drop.count==1 && !world_take_drop(&w,&drop));
    assert(world_set_block(&w,10,65,10,1));
    for(i=2;i<=5;++i) {
        assert(sign_place(&w,10,65,10,(unsigned)i,0,&x,&y,&z));
        assert(x==10+dx[i] && y==65 && z==10+dz[i]);
        assert(world_peek_block(&w,x,y,z)==68 && world_peek_metadata(&w,x,y,z)==i);
        assert(beta_block_selection_box((BetaBlockState){68,(uint8_t)i},&box));
        assert(box.min_y==.28125f && box.max_y==.78125f);
        assert(i==2 ? box.min_z==.875f : i==3 ? box.max_z==.125f : i==4 ? box.min_x==.875f : box.max_x==.125f);
    }
    assert(world_set_block(&w,10,65,10,0)); world_step_physics(&w,4096);
    for(i=2;i<=5;++i) assert(!world_peek_block(&w,10+dx[i],65,10+dz[i]));
    for(i=0;i<4;++i) assert(world_take_drop(&w,&drop) && drop.id==323);
    assert(!world_take_drop(&w,&drop));
    assert(world_set_block(&w,6,63,6,1) && world_set_block(&w,6,64,6,11));
    assert(sign_place(&w,6,63,6,1,180,&x,&y,&z)); /* Replace liquid, as Beta ItemSign does. */
    assert(beta_block_selection_box((BetaBlockState){63,0},&box) && box.min_x==.25f && box.max_y==1);
    assert(world_close(&w)==WORLD_OK);

    assert(world_init(&w,42,1,2)==WORLD_OK);
    assert(world_set_block(&w,16,65,4,1));
    assert(sign_place(&w,16,65,4,4,0,&x,&y,&z) && x==15);
    assert(world_get_chunk(&w,2,2));
    assert(!world_peek_chunk(&w,1,0));
    sign_neighbor_tick(&w,15,65,4); assert(world_peek_block(&w,15,65,4)==68);
    assert(world_close(&w)==WORLD_OK);
}
static void native_tests(void)
{
    World w; char root[128]; int x,y,z; const char (*read)[SIGN_LINE_BYTES];
    char lines[SIGN_LINES][SIGN_LINE_BYTES]={{"A native sign"},{"Second line"},{"\xd0\x9f"},{"123456789012345"}};
    snprintf(root,sizeof(root),"build/sign-test-%d",(int)test_pid()); assert(test_mkdir(root)==0);
    assert(world_create(&w,root,"fixture","Sign fixture",42,1,0,0,16)==WORLD_OK);
    assert(sign_place(&w,4,63,4,1,90,&x,&y,&z) && sign_text_set(&w,x,y,z,lines));
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK && world_get_chunk(&w,0,0));
    read=sign_text_get(&w,x,y,z); assert(read && !memcmp(read,lines,sizeof(lines)));
    assert(world_peek_metadata(&w,x,y,z)==12);
    assert(world_set_block(&w,x,y,z,0) && !sign_text_get(&w,x,y,z));
    assert(world_close(&w)==WORLD_OK);
    assert(world_open(&w,root,"fixture",16)==WORLD_OK && world_get_chunk(&w,0,0));
    assert(!sign_text_get(&w,x,y,z)); assert(world_close(&w)==WORLD_OK);
    assert(world_storage_delete(root,"fixture")==WORLD_OK && test_rmdir(root)==0);
}
static void string_tag(NbtWriter *w,const char *name,const char *value)
{
    NbtTag t={0}; t.type=NBT_STRING; t.name=nbt_span(name); t.value.bytes=nbt_span(value);
    assert(nbt_writer_tag(w,&t)==NBT_OK);
}
static void int_tag(NbtWriter *w,const char *name,int value)
{
    NbtTag t={0}; t.type=NBT_INT; t.name=nbt_span(name); t.value.int_value=value;
    assert(nbt_writer_tag(w,&t)==NBT_OK);
}
typedef struct Canaries { int unknown,inner,foreign,items,text; } Canaries;
static int canary_tag(void *context,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Canaries *c=(Canaries *)context; (void)depth;
    if(event==NBT_VALUE && tag->type==NBT_STRING && tag->name.size==7 && !memcmp(tag->name.data,"Unknown",7))
        c->unknown=tag->value.bytes.size==8 && !memcmp(tag->value.bytes.data,"keep raw",8);
    if(event==NBT_VALUE && tag->type==NBT_INT && tag->name.size==5 && !memcmp(tag->name.data,"Inner",5))
        c->inner=tag->value.int_value==123456;
    if(event==NBT_VALUE && tag->type==NBT_STRING && tag->name.size==2 && !memcmp(tag->name.data,"id",2) &&
       tag->value.bytes.size==7 && !memcmp(tag->value.bytes.data,"Unknown",7)) c->foreign=1;
    if(event==NBT_BEGIN && tag->type==NBT_LIST && tag->name.size==5 && !memcmp(tag->name.data,"Items",5)) ++c->items;
    if(event==NBT_VALUE && tag->type==NBT_STRING && tag->name.size==5 && !memcmp(tag->name.data,"Text1",5)) ++c->text;
    return 1;
}
static void beta_nbt_tests(void)
{
    uint8_t input[2048],*output; size_t size,output_size; NbtWriter writer; NbtTag tag={0};
    Chunk *chunk=(Chunk *)calloc(1,sizeof(*chunk)),*copy=(Chunk *)calloc(1,sizeof(*copy));
    Canaries canaries={0}; BlockEntity *e;
    assert(chunk && copy); nbt_writer_init(&writer,input,sizeof(input),NULL);
    tag.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.name=nbt_span("Level"); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag.type=NBT_LIST; tag.name=nbt_span("TileEntities"); tag.list_type=NBT_COMPOUND; tag.count=2;
    assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    tag=(NbtTag){0}; tag.type=NBT_COMPOUND; assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    string_tag(&writer,"id","Sign"); int_tag(&writer,"x",4); int_tag(&writer,"y",64); int_tag(&writer,"z",4);
    string_tag(&writer,"Text1","1234567890123456789"); string_tag(&writer,"Text2","second");
    string_tag(&writer,"Text3",""); string_tag(&writer,"Text4",""); string_tag(&writer,"Unknown","keep raw");
    tag.name=nbt_span("OpaqueData"); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    int_tag(&writer,"Inner",123456); assert(nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_end(&writer)==NBT_OK);
    tag.name=nbt_span(""); assert(nbt_writer_tag(&writer,&tag)==NBT_OK);
    string_tag(&writer,"id","Unknown"); int_tag(&writer,"x",8); int_tag(&writer,"y",65); int_tag(&writer,"z",8);
    string_tag(&writer,"Reference","preserved unknown tile entity"); assert(nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK && nbt_writer_end(&writer)==NBT_OK);
    assert(nbt_writer_finish(&writer,&size)==NBT_OK && block_entities_read(chunk,input,size));
    e=chunk->entities; assert(e && e->kind==BLOCK_ENTITY_SIGN && !strcmp(e->sign_text[0],"123456789012345"));
    /* An unrelated chunk edit must preserve the original sign's raw text bytes. */
    assert(block_entities_rewrite(chunk,input,size,&output,&output_size));
    assert(output_size==size && !memcmp(input,output,size)); free(output);
    strcpy(e->sign_text[0],"Edited sign"); strcpy(e->sign_text[2],"\xf0\x9f\x98\x80"); e->sign_text_modified=1;
    assert(block_entities_rewrite(chunk,input,size,&output,&output_size));
    assert(nbt_read(output,output_size,NULL,canary_tag,&canaries,NULL)==NBT_OK);
    assert(canaries.unknown && canaries.inner && canaries.foreign && !canaries.items && canaries.text==1);
    assert(block_entities_read(copy,output,output_size) && copy->entities->kind==BLOCK_ENTITY_SIGN);
    assert(!strcmp(copy->entities->sign_text[0],"Edited sign") && !strcmp(copy->entities->sign_text[2],"\xf0\x9f\x98\x80"));
    assert(copy->entities->next->kind==BLOCK_ENTITY_UNKNOWN);
    free(output); block_entities_free(copy); block_entities_free(chunk); free(copy); free(chunk);
}
static void network_tests(void)
{
    World w; char lines[SIGN_LINES][SIGN_LINE_BYTES]={{"From the server"}};
    assert(world_init(&w,42,1,4)==WORLD_OK); w.network_mode=1;
    assert(world_set_block(&w,4,64,4,63));
    assert(!sign_place(&w,4,63,4,1,0,NULL,NULL,NULL));
    assert(!sign_text_set(&w,4,64,4,lines));
    assert(sign_text_receive(&w,4,64,4,lines) && !strcmp(sign_text_get(&w,4,64,4)[0],lines[0]));
    sign_neighbor_tick(&w,4,64,4); assert(world_peek_block(&w,4,64,4)==63);
    assert(world_set_block(&w,4,64,4,0) && !sign_text_get(&w,4,64,4));
    assert(!sign_text_receive(&w,4,64,4,lines));
    assert(world_close(&w)==WORLD_OK);
}
int main(void)
{
    int release=1;
    assert(!gameplay_input_ready(&release,1,0) && release);
    assert(!gameplay_input_ready(&release,0,1) && release);
    assert(gameplay_input_ready(&release,0,0) && !release);
    assert(gameplay_input_ready(&release,1,0));
    text_tests(); placement_tests(); native_tests(); beta_nbt_tests(); network_tests();
    puts("Beta sign placement, support, UTF-8/Java NBT text and native persistence passed"); return 0;
}
