#include "transport.h"
#include "entities.h"
#include "rail.h"
#include "fluid.h"
#include "environment.h"
#include "explosion.h"
#include "../game/player.h"
#include "../game/entity_render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static Chunk *owner(const World *w,float x,float z)
{ return world_peek_chunk(w,(int)floorf(x/16),(int)floorf(z/16)); }
static void dirty(Chunk *c) { c->entities_modified=1; c->dirty_flags|=CHUNK_DIRTY_ENTITIES|CHUNK_DIRTY_SAVE; }
void world_transport_changed(World *w,SavedEntity *e)
{ Chunk *c=e ? owner(w,e->mob.x,e->mob.z) : NULL; if(c) dirty(c); }
SavedEntity *world_minecart_find(World *w,int id)
{
    size_t i;
    for(i=0;i<w->cache_count;++i) { SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e;e=e->next)
            if(e->transport.kind==2 && !e->transport.dead && e->mob.runtime_id==id) return e;
    }
    return NULL;
}
static SavedEntity *spawn(World *w,float x,float y,float z,int kind)
{
    Chunk *c=owner(w,x,z); SavedEntity *e;
    if(w->network_mode || !c || (w->beta_format && !c->beta_raw)) return NULL;
    e=(SavedEntity *)calloc(1,sizeof(*e)); if(!e) return NULL;
    e->transport.kind=kind; e->transport.x_tile=e->transport.y_tile=e->transport.z_tile=-1;
    e->mob.x=x; e->mob.y=y; e->mob.z=z; e->mob.runtime_id=++w->next_entity_id;
    e->transport.previous_x=x; e->transport.previous_y=y; e->transport.previous_z=z;
    e->last_tick=w->tick; e->next=c->saved_entities; c->saved_entities=e; dirty(c); return e;
}
int world_boat_use(World *w,Player *p,InventorySlot *held)
{
    BlockHit hit; SavedEntity *e;
    if(held->id!=333 || held->count<=0 || w->network_mode) return 0;
    hit=player_raycast_sources(p,w,5);
    if(!hit.hit) return 0;
    e=spawn(w,hit.x+.5f,hit.y+(hit.block==78 ? .3f : 1.3f),hit.z+.5f,3);
    if(!e) return 0;
    if(!p->creative && --held->count==0) inventory_clear_slot(held);
    return 1;
}
static float random_float(World *w) { return (float)world_random(w,16777216)/16777216; }
int world_falling_spawn(World *w,int x,int y,int z,int id)
{
    SavedEntity *e=spawn(w,x+.5f,y+.5f,z+.5f,7);
    if(!e)return 0;
    e->transport.falling_block=id;
    /* Remove the source once, atomically with spawning. Scheduled duplicate
     * updates now see air; the persistent entity owns the block until landing. */
    world_set_block(w,x,y,z,0);return 1;
}
int world_tnt_prime(World *w,float x,float y,float z,int fuse)
{
    SavedEntity *e=spawn(w,x,y,z,4); float angle;
    if(!e) return 0;
    angle=random_float(w)*6.283185307f;e->transport.fuse=fuse;
    e->mob.vx=-sinf(angle)*.4f;e->mob.vz=-cosf(angle)*.4f;e->mob.vy=4;
    world_sound(w,"random.fuse",x,y,z,1,1);return 1;
}
float world_entity_gaussian(World *w)
{
    float x,y,s;
    do { x=2*random_float(w)-1; y=2*random_float(w)-1; s=x*x+y*y; } while(s>=1 || s==0);
    return x*sqrtf(-2*logf(s)/s);
}
#define gaussian world_entity_gaussian
int world_dispenser_projectile(World *w,float x,float y,float z,int dx,int dz,int item)
{
    SavedEntity *e=spawn(w,x,y,z,item==262 ? 1 : item==332 ? 5 : 6);
    float length=sqrtf(dx*dx+dz*dz+.01f);
    if(!e) return 0;
    e->transport.player=item==262;
    e->transport.owner_id=-1; /* No shooting entity for a dispenser projectile. */
    e->mob.vx=(dx/length+gaussian(w)*.045f)*22;
    e->mob.vy=(.1f/length+gaussian(w)*.045f)*22;
    e->mob.vz=(dz/length+gaussian(w)*.045f)*22;
    return 1;
}
static void throwable_impact(World *w,SavedEntity *e)
{
    int i,n=1;
    e->transport.dead=1;
    if(e->transport.kind!=6 || world_random(w,8)!=0) return;
    if(world_random(w,32)==0) n=4;
    for(i=0;i<n;++i) world_mob_spawn(w,93,e->mob.x,e->mob.y,e->mob.z);
}
int world_bow_use(World *w,const Player *p,InventorySlot *inventory)
{
    int i; SavedEntity *e; float cp=cosf(p->pitch);
    for(i=0;i<36;++i) if(inventory[i].id==262 && inventory[i].count>0) break;
    if(i==36) return 0; /* Beta has no draw duration or bow durability. */
    e=spawn(w,p->x+cosf(p->yaw)*.16f,p->y+1.52f,p->z+sinf(p->yaw)*.16f,1);
    if(!e) return 0;
    e->transport.player=1;
    e->transport.owner_id=-2; /* Runtime local player; vanilla NBT omits owner. */
    e->mob.vx=(sinf(p->yaw)*cp+gaussian(w)*.0075f)*30;
    e->mob.vy=(sinf(p->pitch)+gaussian(w)*.0075f)*30;
    e->mob.vz=(-cosf(p->yaw)*cp+gaussian(w)*.0075f)*30;
    e->mob.yaw=atan2f(e->mob.vx,-e->mob.vz)*57.2957795f;
    e->mob.pitch=atan2f(e->mob.vy,sqrtf(e->mob.vx*e->mob.vx+e->mob.vz*e->mob.vz))*57.2957795f;
    if(!p->creative && --inventory[i].count==0) inventory_clear_slot(&inventory[i]);
    world_sound(w,"random.bow",p->x,p->y,p->z,1,1/(random_float(w)*.4f+.8f)); return 1;
}
int world_skeleton_arrow(World *w,SavedEntity *skeleton,const Player *target)
{
    float yaw=skeleton->mob.yaw*.01745329252f;
    SavedEntity *e=spawn(w,skeleton->mob.x-cosf(yaw)*.16f,skeleton->mob.y+2.43f,skeleton->mob.z-sinf(yaw)*.16f,1);
    float dx,dy,dz,length;
    if(!e) return 0;
    dx=target->x-skeleton->mob.x;dz=target->z-skeleton->mob.z;
    dy=target->y+PLAYER_BETA_ENTITY_Y_OFFSET+.12f-.2f-e->mob.y+sqrtf(dx*dx+dz*dz)*.2f;
    length=sqrtf(dx*dx+dy*dy+dz*dz);if(length<.001f) length=1;
    e->mob.vx=(dx/length+gaussian(w)*.0075f*12)*12;
    e->mob.vy=(dy/length+gaussian(w)*.0075f*12)*12;
    e->mob.vz=(dz/length+gaussian(w)*.0075f*12)*12;
    e->transport.owner_id=skeleton->mob.runtime_id;
    world_sound(w,"random.bow",skeleton->mob.x,skeleton->mob.y,skeleton->mob.z,1,1/(random_float(w)*.4f+.8f));
    return 1;
}
/* Swept point/AABB intersection, shared by projectiles and cart picking.
 * Tests the entire tick segment; fast arrows cannot pass through a thin door. */
static int intercept(const float p[3],const float v[3],const float lo[3],const float hi[3],float *time)
{
    float near=0,far=*time; int a;
    for(a=0;a<3;++a) {
        if(fabsf(v[a])<1e-8f) { if(p[a]<lo[a] || p[a]>hi[a]) return 0; }
        else {
            float n=(lo[a]-p[a])/v[a],f=(hi[a]-p[a])/v[a],tmp;
            if(n>f) { tmp=n; n=f; f=tmp; }
            if(n>near) near=n;
            if(f<far) far=f;
            if(near>far) return 0;
        }
    }
    if(near>*time || far<0) return 0;
    *time=near; return 1;
}
static void cart_hit(World *w,SavedEntity *e,Player *p,int amount,int creative)
{
    TransportState *s=&e->transport; int n,x=(int)floorf(e->mob.x),y=(int)floorf(e->mob.y),z=(int)floorf(e->mob.z);
    s->damage+=10*amount; s->hit_ticks=10;
    if(!creative && s->damage<=40) return;
    if(s->ridden && p) world_minecart_dismount(w,p);
    if(s->kind==3) {
        world_drop_stack(w,x,y,z,(InventorySlot){5,3,0});
        world_drop_stack(w,x,y,z,(InventorySlot){280,2,0});
        s->dead=1; world_transport_changed(w,e); return;
    }
    world_drop_stack(w,x,y,z,(InventorySlot){328,1,0});
    if(s->type) world_drop_stack(w,x,y,z,(InventorySlot){s->type==1 ? 54 : 61,1,0});
    for(n=0;n<27;++n) if(s->cargo[n].count>0) world_drop_stack(w,x,y,z,s->cargo[n]);
    s->dead=1; world_transport_changed(w,e);
}
void world_transport_damage(World *w,SavedEntity *e,Player *p,int amount)
{ if(e && (e->transport.kind==2||e->transport.kind==3)) cart_hit(w,e,p,amount,0); }
static void arrow_tick(World *w,SavedEntity *e,Player *player,InventorySlot *inventory)
{
    TransportState *s=&e->transport; MobState *m=&e->mob;
    float p[3]={m->x,m->y,m->z},v[3]={m->vx*.05f,m->vy*.05f,m->vz*.05f},best=1;
    int x,y,z,bx=0,by=0,bz=0,block=0,hit_player=0; size_t i; SavedEntity *victim=NULL;
    if(s->shake>0) --s->shake;
    if(s->in_ground) {
        if(world_peek_block(w,s->x_tile,s->y_tile,s->z_tile)==s->in_tile &&
           world_peek_metadata(w,s->x_tile,s->y_tile,s->z_tile)==s->in_data) {
            if(++s->ground_ticks>=1200) s->dead=1;
            if(s->player && !s->shake && player->health>0 && fabsf(m->x-player->x)<.8f &&
               fabsf(m->z-player->z)<.8f && m->y>=player->y-.2f && m->y<=player->y+1.8f &&
               inventory_add_stack(inventory,36,(InventorySlot){262,1,0})==1) {
                s->dead=1; world_sound(w,"random.pop",m->x,m->y,m->z,.2f,2*((random_float(w)-random_float(w))*.7f+1));
            }
            return;
        }
        s->in_ground=0; s->ground_ticks=s->air_ticks=0;
        m->vx*=random_float(w)*.2f; m->vy*=random_float(w)*.2f; m->vz*=random_float(w)*.2f; return;
    }
    ++s->air_ticks;
    { Chunk *target=owner(w,p[0]+v[0],p[2]+v[2]);
      if(!target || (w->beta_format && !target->beta_raw)) { --s->air_ticks; return; } }
    for(x=(int)floorf(fminf(p[0],p[0]+v[0]));x<=(int)floorf(fmaxf(p[0],p[0]+v[0]));++x)
    for(y=(int)floorf(fminf(p[1],p[1]+v[1]));y<=(int)floorf(fmaxf(p[1],p[1]+v[1]));++y)
    for(z=(int)floorf(fminf(p[2],p[2]+v[2]));z<=(int)floorf(fmaxf(p[2],p[2]+v[2]));++z) {
        unsigned id=world_peek_block(w,x,y,z); BetaBlockBox box; float lo[3],hi[3],t=best;
        if(!world_block_def((uint8_t)id)->solid) continue;
        if(!beta_block_selection_box((BetaBlockState){(uint8_t)id,world_peek_metadata(w,x,y,z)},&box))
            box=(BetaBlockBox){0,0,0,1,1,1};
        lo[0]=x+box.min_x; lo[1]=y+box.min_y; lo[2]=z+box.min_z;
        hi[0]=x+box.max_x; hi[1]=y+box.max_y; hi[2]=z+box.max_z;
        if(intercept(p,v,lo,hi,&t)) { best=t; bx=x; by=y; bz=z; block=1; }
    }
    for(i=0;i<w->cache_count;++i) { SavedEntity *other;
        if(w->cache[i]->x<(int)floorf(fminf(p[0],p[0]+v[0])/16)-1 ||
           w->cache[i]->x>(int)floorf(fmaxf(p[0],p[0]+v[0])/16)+1 ||
           w->cache[i]->z<(int)floorf(fminf(p[2],p[2]+v[2])/16)-1 ||
           w->cache[i]->z>(int)floorf(fmaxf(p[2],p[2]+v[2])/16)+1) continue;
        for(other=w->cache[i]->saved_entities;other;other=other->next) if(other!=e &&
            ((other->mob.type && other->mob.health>0) || ((other->transport.kind==2||other->transport.kind==3) && !other->transport.dead))) {
            if(other->mob.runtime_id==s->owner_id && s->air_ticks<5) continue;
            MobState *mob=&other->mob; float half=mob->type==52 ? .7f : mob->type==93 ? .15f : mob->type>=90 ? .45f : .3f;
            float height=mob->type==93 ? .4f : mob->type==52 ? .9f : mob->type>=90 ? 1.3f : 1.8f;
            float lo[3]={mob->x-half-.3f,mob->y-.3f,mob->z-half-.3f};
            float hi[3]={mob->x+half+.3f,mob->y+height+.3f,mob->z+half+.3f},t=best;
            if(other->transport.kind==2) { lo[0]=mob->x-.79f; lo[1]=mob->y-.65f; lo[2]=mob->z-.79f;
                hi[0]=mob->x+.79f; hi[1]=mob->y+.65f; hi[2]=mob->z+.79f; }
            if(intercept(p,v,lo,hi,&t)) { best=t; victim=other; }
        }
    }
    if(s->owner_id!=-2 || s->air_ticks>=5) {
        float lo[3]={player->x-.6f,player->y-.3f,player->z-.6f},hi[3]={player->x+.6f,player->y+2.1f,player->z+.6f},t=best;
        if(player->health>0 && !player->creative && intercept(p,v,lo,hi,&t)) {
            best=t; victim=NULL; hit_player=1;
        }
    }
    if(hit_player) {
        if(s->kind>=5) { throwable_impact(w,e); return; }
        int health=player->health; if(s->player) player_damage(player,4); else player_mob_damage(player,w,4);
        if(player->health<health) { s->dead=1; world_sound(w,"random.drr",m->x,m->y,m->z,1,1); return; }
        m->vx*= -.1f; m->vy*= -.1f; m->vz*= -.1f; s->air_ticks=0; return;
    }
    if(victim) {
        if(s->kind>=5) { if(victim->mob.type) world_mob_hit(w,victim,0); throwable_impact(w,e); return; }
        if(victim->transport.kind>=2) { cart_hit(w,victim,player,4,0); s->dead=1; return; }
        if(world_mob_hit(w,victim,4)) {
            if(victim->mob.type==50 && victim->mob.health==0 && s->owner_id)
                world_drop_stack(w,(int)floorf(victim->mob.x),(int)floorf(victim->mob.y),(int)floorf(victim->mob.z),(InventorySlot){2256+(int)world_random(w,2),1,0});
            s->dead=1; world_sound(w,"random.drr",m->x,m->y,m->z,1,1.2f/(random_float(w)*.2f+.9f)); return;
        }
        m->vx*= -.1f; m->vy*= -.1f; m->vz*= -.1f; s->air_ticks=0; return;
    }
    if(block) {
        if(s->kind>=5) { m->x+=v[0]*best; m->y+=v[1]*best; m->z+=v[2]*best; throwable_impact(w,e); return; }
        float length=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]),t=best-(length>0 ? .05f/length : 0);
        s->x_tile=bx; s->y_tile=by; s->z_tile=bz; s->in_tile=world_peek_block(w,bx,by,bz);
        s->in_data=world_peek_metadata(w,bx,by,bz); s->in_ground=1; s->shake=7;
        m->x+=v[0]*t; m->y+=v[1]*t; m->z+=v[2]*t;
        world_sound(w,"random.drr",m->x,m->y,m->z,1,1.2f/(random_float(w)*.2f+.9f));
    } else { m->x+=v[0]; m->y+=v[1]; m->z+=v[2]; }
    m->yaw=atan2f(m->vx,-m->vz)*57.2957795f; m->pitch=atan2f(m->vy,sqrtf(m->vx*m->vx+m->vz*m->vz))*57.2957795f;
    { float drag=fluid_kind(world_peek_block(w,(int)floorf(m->x),(int)floorf(m->y),(int)floorf(m->z)))==1 ? .8f : .99f;
      m->vx*=drag; m->vy=m->vy*drag-(s->kind==1 ? 1.0f : .6f); m->vz*=drag; }
    if(m->y< -64) s->dead=1;
}
int world_minecart_spawn(World *w,float x,float y,float z,int type)
{
    SavedEntity *e;
    if(type<0 || type>2 || !rail_is(world_peek_block(w,(int)floorf(x),(int)floorf(y),(int)floorf(z)))) return 0;
    e=spawn(w,x,y+.85f,z,2); if(!e) return 0;
    e->transport.type=type; return 1;
}
void world_minecart_dismount(World *w,Player *p)
{
    size_t i;
    for(i=0;i<w->cache_count;++i) { SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e;e=e->next) if(e->transport.kind>=2 && e->transport.ridden) {
            e->transport.ridden=0; p->x=e->mob.x; p->y=e->mob.y+.7f; p->z=e->mob.z;
            p->vx=p->vy=p->vz=0; p->riding=0;
        }
    }
}
int world_transport_interact(World *w,Player *p,InventorySlot *held,int attack)
{
    size_t i; SavedEntity *best=NULL; float time=1,from[3]={p->x,p->y+1.62f,p->z};
    float v[3]={sinf(p->yaw)*cosf(p->pitch)*3,sinf(p->pitch)*3,-cosf(p->yaw)*cosf(p->pitch)*3};
    BlockHit wall=player_raycast(p,w,3); if(wall.hit) time=wall.distance/3;
    for(i=0;i<w->cache_count;++i) { SavedEntity *e;
        if(abs(w->cache[i]->x-(int)floorf(p->x/16))>1 || abs(w->cache[i]->z-(int)floorf(p->z/16))>1) continue;
        for(e=w->cache[i]->saved_entities;e;e=e->next) if((e->transport.kind==2||e->transport.kind==3) && !e->transport.dead) {
            float half=e->transport.kind==3 ? .85f : .59f;
            float lo[3]={e->mob.x-half,e->mob.y-.45f,e->mob.z-half},hi[3]={e->mob.x+half,e->mob.y+.45f,e->mob.z+half},t=time;
            if(intercept(from,v,lo,hi,&t)) { best=e; time=t; }
        }
    }
    if(!best) return 0;
    if(attack) {
        cart_hit(w,best,p,beta_attack_damage(held->id),p->creative);
    } else if(best->transport.type==0) {
        if(best->transport.ridden) world_minecart_dismount(w,p);
        else { world_minecart_dismount(w,p); best->transport.ridden=p->riding=1; }
    } else if(best->transport.type==1) p->cart_inventory=best->mob.runtime_id;
    else if(best->transport.type==2) {
        if(held->id==263 && held->count>0) {
            if(!p->creative && --held->count==0) inventory_clear_slot(held);
            best->transport.fuel+=1200;
        }
        best->transport.push_x=best->mob.x-p->x; best->transport.push_z=best->mob.z-p->z;
    }
    dirty(owner(w,best->mob.x,best->mob.z)); return 1;
}
static int vehicle_blocked(const World *w,float x,float y,float z,float half,float height)
{
    int a,b,c;
    for(a=(int)floorf(x-half);a<=(int)floorf(x+half);++a)
    for(b=(int)floorf(y-height+.001f);b<=(int)floorf(y+height-.001f);++b)
    for(c=(int)floorf(z-half);c<=(int)floorf(z+half);++c) {
        Chunk *chunk=owner(w,(float)a,(float)c); unsigned id; BetaBlockBox box={0,0,0,1,1,1};
        if(!chunk || (w->beta_format && !chunk->beta_raw)) return 1;
        id=world_peek_block(w,a,b,c);
        if(!world_block_def((uint8_t)id)->solid) continue;
        beta_block_selection_box((BetaBlockState){(uint8_t)id,world_peek_metadata(w,a,b,c)},&box);
        if(x+half>a+box.min_x && x-half<a+box.max_x && y+height>b+box.min_y &&
           y-height+.001f<b+box.max_y && z+half>c+box.min_z && z-half<c+box.max_z) return 1;
    }
    return 0;
}
static int cart_blocked(const World *w,float x,float y,float z)
{ return vehicle_blocked(w,x,y,z,.49f,.35f); }
static int vehicle_move_axis(const World *w,MobState *m,int axis,float delta,float half,float height)
{
    int n,steps; float start=axis==0 ? m->x : axis==1 ? m->y : m->z;
    delta=fmaxf(-16,fminf(16,delta)); steps=(int)ceilf(fabsf(delta)/.125f);
    if(steps<1) return 1;
    for(n=1;n<=steps;++n) {
        float target=start+delta*n/steps,x=m->x,y=m->y,z=m->z;
        if(axis==0) x=target; else if(axis==1) y=target; else z=target;
        if(vehicle_blocked(w,x,y,z,half,height)) {
            int k; float lo=0,hi=1,previous=axis==0 ? m->x : axis==1 ? m->y : m->z;
            for(k=0;k<8;++k) {
                float mid=(lo+hi)*.5f,pos=previous+(target-previous)*mid;
                x=m->x; y=m->y; z=m->z;
                if(axis==0) x=pos; else if(axis==1) y=pos; else z=pos;
                if(vehicle_blocked(w,x,y,z,half,height)) hi=mid; else lo=mid;
            }
            target=previous+(target-previous)*lo;
            if(axis==0) m->x=target; else if(axis==1) m->y=target; else m->z=target;
            return 0;
        }
        m->x=x; m->y=y; m->z=z;
    }
    return 1;
}
static int cart_move_axis(const World *w,MobState *m,int axis,float delta)
{ return vehicle_move_axis(w,m,axis,delta,.49f,.35f); }
static void cart_collisions(World *w,SavedEntity *e,Player *p)
{
    size_t i; MobState *m=&e->mob;
    for(i=0;i<w->cache_count;++i) { SavedEntity *other;
        if(abs(w->cache[i]->x-(int)floorf(m->x/16))>1 || abs(w->cache[i]->z-(int)floorf(m->z/16))>1) continue;
        for(other=w->cache[i]->saved_entities;other;other=other->next) if(other!=e && !other->transport.dead &&
            (other->transport.kind==2 || (other->mob.type && other->mob.health>0))) {
            MobState *o=&other->mob; float dx=o->x-m->x,dz=o->z-m->z,dist=sqrtf(dx*dx+dz*dz),nx,nz;
            if(dist<.01f || dist>1.18f || fabsf(m->y-o->y)>.9f) continue;
            nx=dx/dist*fminf(1,1/dist); nz=dz/dist*fminf(1,1/dist); /* .05 blocks/tick = 1 block/sec */
            if(other->transport.kind==2) {
                float vx=(o->vx+m->vx)*.5f,vz=(o->vz+m->vz)*.5f;
                if(other->transport.type==2 && e->transport.type!=2) {
                    m->vx=m->vx*.2f+o->vx-nx; m->vz=m->vz*.2f+o->vz-nz; o->vx*=.7f; o->vz*=.7f;
                } else if(other->transport.type!=2 && e->transport.type==2) {
                    o->vx=o->vx*.2f+m->vx+nx; o->vz=o->vz*.2f+m->vz+nz; m->vx*=.7f; m->vz*=.7f;
                } else {
                    m->vx=m->vx*.2f+vx-nx; m->vz=m->vz*.2f+vz-nz;
                    o->vx=o->vx*.2f+vx+nx; o->vz=o->vz*.2f+vz+nz;
                }
            } else { m->vx-=nx; m->vz-=nz; o->push_x+=nx*.25f; o->push_z+=nz*.25f; }
            world_transport_changed(w,other);
        }
    }
    if(!e->transport.ridden && fabsf(p->y-m->y)<1.2f) {
        float dx=p->x-m->x,dz=p->z-m->z,dist=sqrtf(dx*dx+dz*dz);
        if(dist>.01f && dist<.99f) { float nx=dx/dist*fminf(1,1/dist),nz=dz/dist*fminf(1,1/dist);
            m->vx-=nx; m->vz-=nz; p->push_x+=nx*.25f; p->push_z+=nz*.25f; }
    }
}
static void cart_tick(World *w,SavedEntity *e,Player *p)
{
    MobState *m=&e->mob; TransportState *s=&e->transport;
    float px,py,pz,old_y,nx,nz,dx,dz,len,speed,factor,move_y; int shape,x,y,z; unsigned id,meta;
    if(s->hit_ticks>0) --s->hit_ticks;
    if(s->damage>0) --s->damage;
    x=(int)floorf(m->x); y=(int)floorf(m->y); z=(int)floorf(m->z);
    if(rail_is(world_peek_block(w,x,y-1,z))) --y;
    id=world_peek_block(w,x,y,z); meta=world_peek_metadata(w,x,y,z);
    m->vy-=.8f;
    if(rail_path(w,m->x,m->y,m->z,&px,&py,&pz,&shape)) {
        old_y=py; m->x=px; m->y=py; m->z=pz; m->vy=0;
        move_y=y+(shape>=2 && shape<=5 ? 1 : 0)+.35f;
        if(shape==2) m->vx-=.15625f;
        if(shape==3) m->vx+=.15625f;
        if(shape==4) m->vz+=.15625f;
        if(shape==5) m->vz-=.15625f;
        dx=(float)(rail_ends[shape][1][0]-rail_ends[shape][0][0]); dz=(float)(rail_ends[shape][1][2]-rail_ends[shape][0][2]);
        len=sqrtf(dx*dx+dz*dz); if(m->vx*dx+m->vz*dz<0) { dx=-dx; dz=-dz; }
        speed=sqrtf(m->vx*m->vx+m->vz*m->vz); m->vx=speed*dx/len; m->vz=speed*dz/len;
        if(id==27 && !(meta&8)) { factor=speed<.6f ? 0 : .5f; m->vx*=factor; m->vz*=factor; }
        factor=s->ridden ? .75f : 1;
        nx=m->x+fmaxf(-8,fminf(8,m->vx*factor))*.05f;
        nz=m->z+fmaxf(-8,fminf(8,m->vz*factor))*.05f;
        /* First project onto the current segment (including its slope), then
         * onto the next block's segment. Corners follow MATRIX, not steering. */
        { float ax=x+.5f+rail_ends[shape][0][0]*.5f,az=z+.5f+rail_ends[shape][0][2]*.5f;
          float rx=(rail_ends[shape][1][0]-rail_ends[shape][0][0])*.5f,rz=(rail_ends[shape][1][2]-rail_ends[shape][0][2])*.5f;
          float t=((nx-ax)*rx+(nz-az)*rz)/(rx*rx+rz*rz);
          float extrap=y+.5f+rail_ends[shape][0][1]*.5f+(rail_ends[shape][1][1]-rail_ends[shape][0][1])*t;
          if(rail_ends[shape][1][1]<rail_ends[shape][0][1]) extrap+=1;
          if(rail_ends[shape][1][1]>rail_ends[shape][0][1]) extrap+=.5f;
          py=extrap;
          if(rail_path(w,nx,extrap,nz,&px,&py,&pz,&shape)) { nx=px; nz=pz; }
        }
        s->slope_pitch=atan2f(old_y-py,sqrtf((nx-m->x)*(nx-m->x)+(nz-m->z)*(nz-m->z)))*57.2957795f;
        if(cart_blocked(w,nx,move_y,nz)) m->vx=m->vz=0;
        else { m->x=nx; m->y=py; m->z=nz; }
        factor=s->ridden ? .997f : .96f;
        if(!s->ridden && s->type==2) {
            len=sqrtf(s->push_x*s->push_x+s->push_z*s->push_z);
            if(len>.01f) {
                m->vx=m->vx*.8f+s->push_x/len*.8f; m->vz=m->vz*.8f+s->push_z/len*.8f;
                if(world_random(w,4)==0 && --s->fuel<0) s->push_x=s->push_z=0;
            } else { m->vx*=.9f; m->vz*=.9f; }
        }
        m->vx*=factor; m->vz*=factor;
        speed=sqrtf(m->vx*m->vx+m->vz*m->vz);
        if(speed>0) { float energy=(old_y-m->y); m->vx*=fmaxf(0,speed+energy)/speed; m->vz*=fmaxf(0,speed+energy)/speed; }
        if((int)floorf(m->x)!=x || (int)floorf(m->z)!=z) {
            speed=sqrtf(m->vx*m->vx+m->vz*m->vz);
            m->vx=speed*((int)floorf(m->x)-x); m->vz=speed*((int)floorf(m->z)-z);
        }
        if(id==27 && (meta&8)) {
            speed=sqrtf(m->vx*m->vx+m->vz*m->vz);
            if(speed>.2f) { m->vx+=m->vx/speed*1.2f; m->vz+=m->vz/speed*1.2f; }
            else if((meta&7)==1) {
                if(world_block_def(world_peek_block(w,x-1,y,z))->opaque) m->vx=.4f;
                else if(world_block_def(world_peek_block(w,x+1,y,z))->opaque) m->vx=-.4f;
            } else if((meta&7)==0) {
                if(world_block_def(world_peek_block(w,x,y,z-1))->opaque) m->vz=.4f;
                else if(world_block_def(world_peek_block(w,x,y,z+1))->opaque) m->vz=-.4f;
            }
        }
        if(id==28) rail_update(w,x,y,z);
    } else {
        dx=fmaxf(-8,fminf(8,m->vx))*.05f; dz=fmaxf(-8,fminf(8,m->vz))*.05f;
        if(!cart_move_axis(w,m,0,dx)) m->vx=0;
        if(!cart_move_axis(w,m,2,dz)) m->vz=0;
        if(!cart_move_axis(w,m,1,m->vy*.05f)) { m->vy=0; m->vx*=.5f; m->vz*=.5f; }
        m->vx*=.95f; m->vy*=.95f; m->vz*=.95f;
    }
    if(m->vx*m->vx+m->vz*m->vz>.0001f) m->yaw=atan2f(-m->vz,-m->vx)*57.2957795f;
    m->pitch=0;
    cart_collisions(w,e,p);
    if(s->ridden) { p->x=m->x; p->y=m->y-.3f; p->z=m->z; p->vx=p->vy=p->vz=0; }
    if(m->y< -64) { if(s->ridden) world_minecart_dismount(w,p); s->dead=1; }
}
static void boat_tick(World *w,SavedEntity *e,Player *p)
{
    MobState *m=&e->mob; TransportState *s=&e->transport;
    float submerged=0,speed,turn; int slice,x,y,z,hit=0;
    size_t i;
    if(s->hit_ticks>0) --s->hit_ticks;
    if(s->damage>0) --s->damage;
    /* EntityBoat samples five slices of its 1.5 x .6 bounding box. */
    for(slice=0;slice<5;++slice) {
        int water=0;
        float low=m->y-.3f+slice*.12f-.125f,high=low+.12f;
        for(x=(int)floorf(m->x-.75f);x<=(int)floorf(m->x+.75f);++x)
        for(y=(int)floorf(low);y<=(int)floorf(high);++y)
        for(z=(int)floorf(m->z-.75f);z<=(int)floorf(m->z+.75f);++z)
            if(fluid_kind(world_peek_block(w,x,y,z))==1) {
                unsigned level=world_peek_metadata(w,x,y,z);
                float surface=y+1-(float)((level>=8 ? 0 : level)+1)/9;
                if(high>y && low<surface) water=1;
            }
        if(water) submerged+=.2f;
    }
    if(submerged<.999f) m->vy+=.8f*(submerged*2-1);
    else { if(m->vy<0) m->vy*=.5f; m->vy+=.14f; }
    if(s->ridden) { m->vx+=p->vx*.2f; m->vz+=p->vz*.2f; }
    m->vx=fmaxf(-8,fminf(8,m->vx)); m->vz=fmaxf(-8,fminf(8,m->vz));
    if(m->on_ground) { m->vx*=.5f; m->vy*=.5f; m->vz*=.5f; }
    speed=sqrtf(m->vx*m->vx+m->vz*m->vz);
    m->on_ground=0;
    if(!vehicle_move_axis(w,m,1,m->vy*.05f,.75f,.3f)) { if(m->vy<0) m->on_ground=1; m->vy=0; }
    if(!vehicle_move_axis(w,m,0,m->vx*.05f,.75f,.3f)) { m->vx=0; hit=1; }
    if(!vehicle_move_axis(w,m,2,m->vz*.05f,.75f,.3f)) { m->vz=0; hit=1; }
    if(hit && speed>3) { cart_hit(w,e,p,5,0); return; }
    m->vx*=.99f; m->vy*=.95f; m->vz*=.99f;
    if((s->previous_x-m->x)*(s->previous_x-m->x)+(s->previous_z-m->z)*(s->previous_z-m->z)>.001f) {
        turn=atan2f(s->previous_z-m->z,s->previous_x-m->x)*57.2957795f-m->yaw;
        while(turn>=180) turn-=360;
        while(turn< -180) turn+=360;
        m->yaw+=fmaxf(-20,fminf(20,turn));
    }
    m->pitch=0;
    for(i=0;i<w->cache_count;++i) {
        SavedEntity *other;
        if(abs(w->cache[i]->x-(int)floorf(m->x/16))>1 || abs(w->cache[i]->z-(int)floorf(m->z/16))>1) continue;
        for(other=w->cache[i]->saved_entities;other;other=other->next)
            if(other!=e && other->transport.kind==3 && !other->transport.dead && fabsf(m->y-other->mob.y)<.6f) {
                float dx=other->mob.x-m->x,dz=other->mob.z-m->z,d=fmaxf(fabsf(dx),fabsf(dz));
                if(fabsf(dx)<1.7f && fabsf(dz)<1.7f && d>=.01f) {
                    float f=fminf(1,1/sqrtf(d))/sqrtf(d);
                    m->vx-=dx*f; m->vz-=dz*f; other->mob.vx+=dx*f; other->mob.vz+=dz*f;
                    world_transport_changed(w,other);
                }
            }
    }
    if(s->ridden) {
        float a=m->yaw*.01745329252f;
        p->x=m->x+cosf(a)*.4f; p->y=m->y-.3f; p->z=m->z+sinf(a)*.4f;
        p->vx=p->vy=p->vz=0;
    }
    if(m->y< -64) { if(s->ridden) world_minecart_dismount(w,p); s->dead=1; }
}
void world_transport_tick(World *w,Player *p,InventorySlot *inventory)
{
    size_t i;
    if(w->network_mode) return;
    for(i=0;i<w->cache_count;++i) {
        Chunk *c=w->cache[i]; SavedEntity **link=&c->saved_entities;
        while(*link) {
            SavedEntity *e=*link; Chunk *target;
            if(!e->transport.kind || e->last_tick==w->tick) { link=&e->next; continue; }
            e->last_tick=w->tick;
            e->transport.previous_x=e->mob.x; e->transport.previous_y=e->mob.y; e->transport.previous_z=e->mob.z;
            if(e->transport.kind==1 || e->transport.kind==5 || e->transport.kind==6) arrow_tick(w,e,p,inventory);
            else if(e->transport.kind==3) boat_tick(w,e,p);
            else if(e->transport.kind==4 || e->transport.kind==7) {
                MobState *m=&e->mob;m->vy-=.8f;m->on_ground=0;
                if(!vehicle_move_axis(w,m,1,m->vy*.05f,.49f,.49f)) { if(m->vy<0)m->on_ground=1;m->vy*= -.5f; }
                vehicle_move_axis(w,m,0,m->vx*.05f,.49f,.49f);vehicle_move_axis(w,m,2,m->vz*.05f,.49f,.49f);
                m->vx*=.98f;m->vy*=.98f;m->vz*=.98f;
                if(m->on_ground) {m->vx*=.7f;m->vz*=.7f;}
                if(e->transport.kind==4) {
                    if(e->transport.fuse--<=0) {e->transport.dead=1;world_explode(w,p,m->x,m->y,m->z,4,0);}
                } else {
                    int x=(int)floorf(m->x),y=(int)floorf(m->y),z=(int)floorf(m->z);
                    if(m->on_ground || ++e->transport.fall_time>100) {
                        unsigned at=world_peek_block(w,x,y,z),below=world_peek_block(w,x,y-1,z);
                        e->transport.dead=1;
                        if(!m->on_ground || y<=0 || (at && at!=51 && !fluid_kind(at)) || !below || below==51 || fluid_kind(below) ||
                           !world_set_block(w,x,y,z,(uint8_t)e->transport.falling_block))
                            world_item_spawn_at(w,m->x,m->y,m->z,(InventorySlot){e->transport.falling_block,1,0});
                    }
                }
            } else cart_tick(w,e,p);
            dirty(c);
            if(e->transport.dead) {
                /* Damage/explosions can prepend drops or newly primed TNT to
                 * this list while the current entity is being simulated. */
                SavedEntity **actual=&c->saved_entities;while(*actual && *actual!=e)actual=&(*actual)->next;
                if(*actual)*actual=e->next;
                free(e->raw);free(e);link=&c->saved_entities;continue;
            }
            target=owner(w,e->mob.x,e->mob.z);
            if(target && target!=c && (!w->beta_format || target->beta_raw)) {
                SavedEntity **actual=&c->saved_entities;while(*actual && *actual!=e)actual=&(*actual)->next;
                if(*actual)*actual=e->next;
                e->next=target->saved_entities;target->saved_entities=e;dirty(target);link=&c->saved_entities;continue;
            }
            link=&e->next;
        }
    }
}
int world_transport_visible(World *w,RenderEntity *out,int capacity)
{
    size_t i; int n=0;
    for(i=0;i<w->cache_count && n<capacity;++i) { SavedEntity *e;
        for(e=w->cache[i]->saved_entities;e && n<capacity;e=e->next) if(e->transport.kind && !e->transport.dead) {
            RenderEntity *r=&out[n++]; if(!e->mob.runtime_id) e->mob.runtime_id=++w->next_entity_id;
            if(r->id!=e->mob.runtime_id) memset(r,0,sizeof(*r));
            r->active=1; r->id=e->mob.runtime_id; r->type=999+e->transport.kind;
            r->x=e->mob.x; r->y=e->mob.y; r->z=e->mob.z;
            r->yaw=e->mob.yaw; r->pitch=e->transport.kind==1 || e->transport.kind==5 || e->transport.kind==6 ? e->mob.pitch : e->transport.slope_pitch; r->color=e->transport.kind==7 ? e->transport.falling_block : e->transport.type;
            r->local_interpolation=1; r->phase=0; r->fuse=e->transport.fuse;r->fire=e->mob.fire;
            r->previous_x=e->transport.previous_x; r->previous_y=e->transport.previous_y; r->previous_z=e->transport.previous_z;
        }
    }
    return n;
}
static int number(NbtWriter *w,NbtType type,const char *name,double value)
{
    NbtTag t={0}; t.type=type; t.name=nbt_span(name);
    if(type==NBT_BYTE) t.value.byte=(int8_t)value;
    else if(type==NBT_SHORT) t.value.short_value=(int16_t)value;
    else if(type==NBT_INT) t.value.int_value=(int32_t)value;
    else if(type==NBT_FLOAT) t.value.float_value=(float)value;
    else t.value.double_value=value;
    return nbt_writer_tag(w,&t)==NBT_OK;
}
static int list(NbtWriter *w,const char *name,NbtType type,double x,double y,double z,int count)
{
    NbtTag t={0}; double p[3]={x,y,z}; int n;
    t.type=NBT_LIST; t.name=nbt_span(name); t.list_type=type; t.count=count;
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    for(n=0;n<count;++n) if(!number(w,type,"",p[n])) return 0;
    return nbt_writer_end(w)==NBT_OK;
}
static int fields(NbtWriter *w,const SavedEntity *e)
{
    NbtTag t={0}; const MobState *m=&e->mob; const TransportState *s=&e->transport; int n;
    t.type=NBT_STRING; t.name=nbt_span("id"); t.value.bytes=nbt_span(s->kind==1 ? "Arrow" : s->kind==3 ? "Boat" : s->kind==4 ? "PrimedTnt" : s->kind==5 ? "Snowball" : s->kind==6 ? "Egg" : s->kind==7 ? "FallingSand" : "Minecart");
    if(nbt_writer_tag(w,&t)!=NBT_OK || !list(w,"Pos",NBT_DOUBLE,m->x,m->y,m->z,3) ||
       !list(w,"Motion",NBT_DOUBLE,m->vx/20,m->vy/20,m->vz/20,3) ||
       !list(w,"Rotation",NBT_FLOAT,s->kind==1 || s->kind==5 || s->kind==6 ? 180-m->yaw : m->yaw,m->pitch,0,2) ||
       !number(w,NBT_FLOAT,"FallDistance",0) || !number(w,NBT_SHORT,"Fire",m->fire) ||
       !number(w,NBT_SHORT,"Air",300) || !number(w,NBT_BYTE,"OnGround",m->on_ground)) return 0;
    if(s->kind==7)return number(w,NBT_BYTE,"Tile",s->falling_block);
    if(s->kind==1 || s->kind==5 || s->kind==6) return number(w,NBT_SHORT,"xTile",s->x_tile) && number(w,NBT_SHORT,"yTile",s->y_tile) &&
        number(w,NBT_SHORT,"zTile",s->z_tile) && number(w,NBT_BYTE,"inTile",s->in_tile) && (s->kind!=1 || number(w,NBT_BYTE,"inData",s->in_data)) &&
        number(w,NBT_BYTE,"shake",s->shake) && number(w,NBT_BYTE,"inGround",s->in_ground) && (s->kind!=1 || number(w,NBT_BYTE,"player",s->player));
    if(s->kind==3) return 1; /* Beta Boat adds no fields to Entity NBT. */
    if(s->kind==4) return number(w,NBT_BYTE,"Fuse",s->fuse);
    if(!number(w,NBT_INT,"Type",s->type)) return 0;
    if(s->type==2 && (!number(w,NBT_DOUBLE,"PushX",s->push_x) || !number(w,NBT_DOUBLE,"PushZ",s->push_z) || !number(w,NBT_SHORT,"Fuel",s->fuel))) return 0;
    if(s->type!=1) return 1;
    t=(NbtTag){0}; t.type=NBT_LIST; t.list_type=NBT_COMPOUND; t.name=nbt_span("Items");
    for(n=0;n<27;++n) if(s->cargo[n].count>0) ++t.count;
    if(nbt_writer_tag(w,&t)!=NBT_OK) return 0;
    for(n=0;n<27;++n) if(s->cargo[n].count>0) {
        NbtTag compound={0}; InventorySlot item=s->cargo[n]; compound.type=NBT_COMPOUND;
        if(nbt_writer_tag(w,&compound)!=NBT_OK || !number(w,NBT_BYTE,"Slot",n) || !number(w,NBT_SHORT,"id",item.id) ||
           !number(w,NBT_BYTE,"Count",item.count) || !number(w,NBT_SHORT,"Damage",item.damage) || nbt_writer_end(w)!=NBT_OK) return 0;
    }
    return nbt_writer_end(w)==NBT_OK;
}
typedef struct Rewrite { NbtWriter *w; const SavedEntity *e; int skip; unsigned skip_depth; } Rewrite;
static int known(NbtSpan name)
{
    static const char *names[]={"id","Pos","Motion","Rotation","FallDistance","Fire","Air","OnGround","xTile","yTile","zTile","inTile","inData","shake","inGround","player","Type","PushX","PushZ","Fuel","Items","Fuse","Tile"};
    unsigned i; for(i=0;i<sizeof(names)/sizeof(names[0]);++i) if(name.size==strlen(names[i]) && !memcmp(name.data,names[i],name.size)) return 1;
    return 0;
}
static int rewrite(void *ctx,NbtEvent event,const NbtTag *tag,unsigned depth)
{
    Rewrite *r=(Rewrite *)ctx;
    if(r->skip) { if(event==NBT_FINISH && depth==r->skip_depth) r->skip=0; return 1; }
    if(depth==1 && known(tag->name)) { if(event==NBT_BEGIN) { r->skip=1; r->skip_depth=depth; } return 1; }
    if(event==NBT_FINISH && depth==0 && !fields(r->w,r->e)) return 0;
    return (event==NBT_FINISH ? nbt_writer_end(r->w) : nbt_writer_tag(r->w,tag))==NBT_OK;
}
int world_transport_write(NbtWriter *w,const SavedEntity *e)
{
    NbtTag t={0}; Rewrite r={w,e,0,0};
    if(e->raw) return nbt_read(e->raw,e->raw_size,NULL,rewrite,&r,NULL)==NBT_OK;
    t.type=NBT_COMPOUND;
    return nbt_writer_tag(w,&t)==NBT_OK && fields(w,e) && nbt_writer_end(w)==NBT_OK;
}
