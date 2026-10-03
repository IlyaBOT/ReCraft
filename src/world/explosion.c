#include "explosion.h"
#include "entities.h"
#include "environment.h"
#include "fire.h"
#include "../game/player.h"
#include "../game/mining.h"
#include <math.h>
#include <stdlib.h>
float beta_blast_resistance(unsigned id)
{
    switch(id) {
    /* Block.setResistance multiplies by 3, getExplosionResistance divides by
     * 5. Unspecified resistance defaults to hardness in this version. */
    case 7:return 3600000;case 49:return 1200;case 8:case 9:case 11:return 100;
    case 1:case 4:case 41:case 42:case 43:case 44:case 45:case 48:case 57:case 67:case 84:return 6;
    case 5:case 14:case 15:case 16:case 21:case 22:case 53:case 56:case 73:case 74:case 85:return 3;
    case 17:return 2;case 23:case 61:case 62:return 3.5f;
    case 54:case 58:return 2.5f;case 64:case 96:return 3;
    case 71:case 52:return 5;case 0:case 10:case 46:case 90:return 0;
    default:return fmaxf(0,mining_hardness(id));
    }
}
static int loaded(const World *w,int x,int z)
{ Chunk *c=world_peek_chunk(w,x>>4,z>>4);return c && (!w->beta_format||c->beta_raw); }
static int clear_ray(const World *w,float x,float y,float z,float tx,float ty,float tz)
{
    float origin[3]={x,y,z},direction[3]={tx-x,ty-y,tz-z},next[3],delta[3];
    int cell[3]={(int)floorf(x),(int)floorf(y),(int)floorf(z)},step[3],axis,n;
    /* Voxel traversal plus segment/AABB tests, so a glancing ray cannot skip
     * thin doors or slab boundaries as a fixed-distance sampler could. */
    for(axis=0;axis<3;++axis) {
        step[axis]=direction[axis]>0?1:-1;
        if(fabsf(direction[axis])<1e-8f)next[axis]=delta[axis]=1e30f;
        else {next[axis]=(cell[axis]+(step[axis]>0)-origin[axis])/direction[axis];delta[axis]=fabsf(1/direction[axis]);}
    }
    for(n=0;n<256;++n) {
        unsigned id=world_peek_block(w,cell[0],cell[1],cell[2]);BetaBlockBox b;
        if(!loaded(w,cell[0],cell[2]))return 0;
        if(world_block_def((uint8_t)id)->solid) {
            float near=0,far=1,lo[3],hi[3];int intersects=1;
            if(!beta_block_selection_box((BetaBlockState){(uint8_t)id,world_peek_metadata(w,cell[0],cell[1],cell[2])},&b))b=(BetaBlockBox){0,0,0,1,1,1};
            lo[0]=b.min_x+cell[0];lo[1]=b.min_y+cell[1];lo[2]=b.min_z+cell[2];
            hi[0]=b.max_x+cell[0];hi[1]=b.max_y+cell[1];hi[2]=b.max_z+cell[2];
            for(axis=0;axis<3;++axis) {
                if(fabsf(direction[axis])<1e-8f){if(origin[axis]<lo[axis]||origin[axis]>hi[axis])intersects=0;}
                else {float enter=(lo[axis]-origin[axis])/direction[axis],leave=(hi[axis]-origin[axis])/direction[axis];
                    if(enter>leave){float swap=enter;enter=leave;leave=swap;}near=fmaxf(near,enter);far=fminf(far,leave);}
            }
            if(intersects&&near<=far&&far>0&&near<1)return 0;
        }
        axis=next[0]<next[1]?(next[0]<next[2]?0:2):(next[1]<next[2]?1:2);
        if(next[axis]>1)return 1;
        cell[axis]+=step[axis];next[axis]+=delta[axis];
    }
    return 0;
}
static float exposure(const World *w,float x,float y,float z,float px,float py,float pz,float width,float height)
{
    int a,b,c,clear=0,total=0; float step_x=1/(width*2+1),step_y=1/(height*2+1);
    for(a=0;a*step_x<=1;++a) for(b=0;b*step_y<=1;++b) for(c=0;c*step_x<=1;++c) {
        clear+=clear_ray(w,px-width*.5f+a*step_x*width,py+b*step_y*height,pz-width*.5f+c*step_x*width,x,y,z); ++total;
    }
    return total?(float)clear/total:0;
}
static float impact(const World *w,float x,float y,float z,float radius,float px,float py,float pz,float width,float height,float y_offset,float velocity[3])
{
    float dx=px-x,dy=py+y_offset-y,dz=pz-z,distance=sqrtf(dx*dx+dy*dy+dz*dz),value;
    if(distance>radius) return 0;
    value=(1-distance/radius)*exposure(w,x,y,z,px,py,pz,width,height);
    if(distance>.0001f) { velocity[0]=dx/distance*value*20;velocity[1]=dy/distance*value*20;velocity[2]=dz/distance*value*20; }
    else { velocity[0]=velocity[2]=0;velocity[1]=value*20; }
    return value;
}
void world_explode(World *w,Player *player,float x,float y,float z,float power,int flaming)
{
    enum { SIDE=65,VOLUME=SIDE*SIDE*SIDE };
    unsigned char *cells; int a,b,c,bx=(int)floorf(x)-32,by=(int)floorf(y)-32,bz=(int)floorf(z)-32;
    int processing; size_t index; float radius=power*2;
    if(!w || w->network_mode || !isfinite(x)||!isfinite(y)||!isfinite(z)||power<=0||power>6) return;
    cells=(unsigned char *)calloc(VOLUME,1); if(!cells) { w->error=WORLD_ERROR_OUT_OF_MEMORY;return; }
    /* Beta traces the shell of a 16^3 cube, with resistance at 0.3-block steps. */
    for(a=0;a<16;++a) for(b=0;b<16;++b) for(c=0;c<16;++c) if(a==0||a==15||b==0||b==15||c==0||c==15) {
        float dx=(float)a/15*2-1,dy=(float)b/15*2-1,dz=(float)c/15*2-1;
        float length=sqrtf(dx*dx+dy*dy+dz*dz),px=x,py=y,pz=z;
        float strength=power*(.7f+(float)world_random(w,16777216)/16777216*.6f);
        dx/=length;dy/=length;dz/=length;
        while(strength>0) {
            int wx=(int)floorf(px),wy=(int)floorf(py),wz=(int)floorf(pz),lx=wx-bx,ly=wy-by,lz=wz-bz;
            unsigned id;
            if((unsigned)wy>=128 || !loaded(w,wx,wz)) break;
            id=world_peek_block(w,wx,wy,wz);
            if(id) strength-=(beta_blast_resistance(id)+.3f)*.3f;
            if(strength>0 && (unsigned)lx<SIDE && (unsigned)ly<SIDE && (unsigned)lz<SIDE) cells[(ly*SIDE+lz)*SIDE+lx]=1;
            px+=dx*.3f;py+=dy*.3f;pz+=dz*.3f;strength-=.225f;
        }
    }
    if(player) {
        float v[3],value=impact(w,x,y,z,radius,player->x,player->y,player->z,.6f,1.8f,PLAYER_BETA_ENTITY_Y_OFFSET,v);
        if(value>0) {
            player_damage(player,(int)((value*value+value)*.5f*8*radius+1));
            player->push_x+=v[0];player->vy+=v[1];player->push_z+=v[2];
        }
    }
    for(index=0;index<w->cache_count;++index) { SavedEntity *e;
        for(e=w->cache[index]->saved_entities;e;e=e->next) {
            float v[3],value,width=.6f,height=1.8f,y_offset=0;
            float px=e->item_entity?e->item.x:e->mob.x,py=e->item_entity?e->item.y:e->mob.y,pz=e->item_entity?e->item.z:e->mob.z;
            if(!e->item_entity && !e->mob.type && !e->transport.kind) continue;
            if(e->item_entity){width=height=.25f;y_offset=.125f;}
            else if(e->mob.type)mob_dimensions(e->mob.type,&width,&height);
            else if(e->transport.kind==1){width=height=.5f;}
            else if(e->transport.kind==2){width=.98f;height=.7f;y_offset=.35f;}
            else if(e->transport.kind==3){width=1.5f;height=.6f;y_offset=.3f;}
            else if(e->transport.kind==4){width=height=.98f;y_offset=.49f;}
            value=impact(w,x,y,z,radius,px,py-y_offset,pz,width,height,y_offset,v);
            if(value<=0) continue;
            if(e->item_entity) { e->item.health-=(int)((value*value+value)*.5f*8*radius+1);e->item.vx+=v[0];e->item.vy+=v[1];e->item.vz+=v[2]; }
            else {
                int damage=(int)((value*value+value)*.5f*8*radius+1);
                if(e->mob.type) world_mob_hit(w,e,damage);
                else if(e->transport.kind==2 || e->transport.kind==3) world_transport_damage(w,e,player,damage);
                e->mob.vx+=v[0];e->mob.vy+=v[1];e->mob.vz+=v[2];
            }
            w->cache[index]->entities_modified=1;w->cache[index]->dirty_flags|=CHUNK_DIRTY_SAVE|CHUNK_DIRTY_ENTITIES;
        }
    }
    processing=w->physics_processing;w->physics_processing=1;
    for(a=0;a<SIDE;++a) for(b=0;b<SIDE;++b) for(c=0;c<SIDE;++c) if(cells[(b*SIDE+c)*SIDE+a]) {
        int wx=bx+a,wy=by+b,wz=bz+c; BetaBlockState state={world_peek_block(w,wx,wy,wz),world_peek_metadata(w,wx,wy,wz)};
        if(!state.id) continue;
        if(state.id!=46 && world_random(w,100)<30) {
            int tool=mining_can_harvest(278,state.id)?278:mining_can_harvest(277,state.id)?277:279;
            world_drop_stack(w,wx,wy,wz,mining_drop(state,tool,world_random(w,UINT32_MAX>>1)));
        }
        world_set_block(w,wx,wy,wz,0);
        if(state.id==46) world_tnt_prime(w,wx+.5f,wy+.5f,wz+.5f,(int)world_random(w,20)+10);
    }
    if(flaming) for(a=0;a<SIDE;++a) for(b=0;b<SIDE;++b) for(c=0;c<SIDE;++c)
        if(cells[(b*SIDE+c)*SIDE+a] && !world_random(w,3)) world_ignite(w,bx+a,by+b,bz+c);
    w->physics_processing=processing;
    if(!processing) world_finish_light_updates(w);
    world_sound(w,"random.old_explode",x,y,z,4,(1+((float)world_random(w,100)-(float)world_random(w,100))/100*.2f)*.7f);
    free(cells);
}
