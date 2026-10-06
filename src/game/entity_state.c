#include "entity_render.h"
#include "../world/collision.h"
#include "player.h"
#include <math.h>

static float angle_delta(float a,float b)
{ float d=a-b;while(d>180)d-=360;while(d< -180)d+=360;return d; }
void entity_remote_pose(RenderEntity *e,float x,float y,float z,float yaw,float pitch)
{
    e->x=x;e->y=y;e->z=z;e->yaw=yaw;e->pitch=pitch;
    e->lerp_remaining=e->type==1002 ? .35f : e->type==1001 ? .25f : .15f;
}
int entity_rider_position(const RenderEntity *entities,int count,int vehicle,int rendered,float *x,float *y,float *z)
{
    int i;
    for(i=0;i<count;++i) if(entities[i].active && entities[i].id==vehicle) {
        const RenderEntity *e=&entities[i];float yaw=rendered ? e->draw_yaw : e->yaw;
        *x=rendered ? e->draw_x : e->x;*y=(rendered ? e->draw_y : e->y)-.8f;*z=rendered ? e->draw_z : e->z;
        if(e->type==1002) { *x+=cosf(yaw*.01745329252f)*.4f;*z+=sinf(yaw*.01745329252f)*.4f; }
        return 1;
    }
    return 0;
}
void entity_render_update(RenderEntity *entities,int count,float dt,float fraction)
{
    int i;dt=fmaxf(0,fminf(.25f,dt));fraction=fmaxf(0,fminf(1,fraction));
    for(i=0;i<count;++i) if(entities[i].active) {
        RenderEntity *e=&entities[i];float old_x=e->draw_x,old_z=e->draw_z;
        int snap=!e->positioned || (e->x-old_x)*(e->x-old_x)+(e->y-e->draw_y)*(e->y-e->draw_y)+(e->z-old_z)*(e->z-old_z)>64;
        if(e->local_interpolation) {
            e->phase=fraction;e->draw_x=e->previous_x+(e->x-e->previous_x)*fraction;
            e->draw_y=e->previous_y+(e->y-e->previous_y)*fraction;e->draw_z=e->previous_z+(e->z-e->previous_z)*fraction;
        } else if(snap) { e->draw_x=e->x;e->draw_y=e->y;e->draw_z=e->z; }
        else if(dt>0) {
            float a=e->lerp_remaining>0 ? fminf(1,dt/e->lerp_remaining) : 1-expf(-10*dt);
            e->draw_x+=(e->x-e->draw_x)*a;e->draw_y+=(e->y-e->draw_y)*a;e->draw_z+=(e->z-e->draw_z)*a;
            e->draw_yaw+=angle_delta(e->yaw,e->draw_yaw)*a;e->draw_pitch+=(e->pitch-e->draw_pitch)*a;
            e->lerp_remaining=fmaxf(0,e->lerp_remaining-dt);
        }
        if(snap) { e->draw_yaw=e->yaw;e->draw_pitch=e->pitch; }
        if(e->local_interpolation) { e->draw_yaw=e->yaw;e->draw_pitch=e->pitch; }
        if(dt>0) {
            /* EntityLiving limbYaw approaches displacement*4 by .4 per tick.
             * Phase alone must not hold the limbs in a walking pose at rest. */
            float target=snap ? 0 : fminf(1,sqrtf((e->draw_x-old_x)*(e->draw_x-old_x)+(e->draw_z-old_z)*(e->draw_z-old_z))*.2f/dt);
            e->walk_amount+=(target-e->walk_amount)*(1-powf(.6f,dt*20));
            e->walk+=e->walk_amount*dt*20;e->age+=dt;
            if(e->hurt>0)e->hurt=(int)fmaxf(0,e->hurt-dt*1000);
        }
        e->positioned=1;
    }
    for(i=0;i<count;++i)if(entities[i].active&&entities[i].vehicle_id) {
        RenderEntity *e=&entities[i];
        entity_rider_position(entities,count,e->vehicle_id-1,1,&e->draw_x,&e->draw_y,&e->draw_z);
    }
}
void entity_network_collide(RenderEntity *entities,int count,Player *p)
{
    int i;if(p->riding)return;
    for(i=0;i<count;++i) {
        RenderEntity *e=&entities[i];float width=.6f,height=1.8f,dx,dz,d;
        if(!e->active||e->death||e->type>=1003)continue;
        if(e->type==1001){width=.98f;height=.7f;}else if(e->type==1002){width=1.5f;height=.6f;}
        else if(e->type==52){width=1.4f;height=.9f;}else if(e->type>=90){width=.9f;height=1.3f;}
        dx=p->x-e->x;dz=p->z-e->z;
        if(fabsf(dx)>(width+.6f)*.5f || fabsf(dz)>(width+.6f)*.5f || p->y>=e->y+height || p->y+1.8f<=e->y)continue;
        d=fmaxf(fabsf(dx),fabsf(dz));if(d<.01f)continue;d=sqrtf(d);
        /* Entity.applyEntityCollision: .05 blocks/tick, capped reciprocal.
         * Predict our impulse only; the server owns the remote entity. */
        p->push_x+=dx/d*fminf(1,1/d);p->push_z+=dz/d*fminf(1,1/d);
    }
}
void entity_network_tick(RenderEntity *entities,int count,World *w)
{
    int i;
    for(i=0;i<count;++i) if(entities[i].active && entities[i].type==1008) {
        RenderEntity *e=&entities[i];int axis;float *pos[3]={&e->x,&e->y,&e->z},*v[3]={&e->vx,&e->vy,&e->vz};
        Chunk *c=world_peek_chunk(w,(int)floorf(e->x/16),(int)floorf(e->z/16));
        if(!c || !c->network_received)continue; /* Never fall through unloaded terrain. */
        e->previous_x=e->x;e->previous_y=e->y;e->previous_z=e->z;e->local_interpolation=1;e->vy-=.8f;e->on_ground=0;
        for(axis=1;axis>=0;--axis) {
            WorldAabb b={{e->x-.125,e->y-.125,e->z-.125},{e->x+.125,e->y+.125,e->z+.125}};
            double delta=*v[axis]*.05,clipped=world_clip_axis(w,&b,axis,delta,0);
            *pos[axis]+=(float)clipped;
            if(fabs(clipped-delta)>1e-6) { if(axis==1 && delta<0)e->on_ground=1;else *v[axis]=0; }
        }
        { WorldAabb b={{e->x-.125,e->y-.125,e->z-.125},{e->x+.125,e->y+.125,e->z+.125}};
          double delta=e->vz*.05,clipped=world_clip_axis(w,&b,2,delta,0);e->z+=(float)clipped;if(fabs(clipped-delta)>1e-6)e->vz=0; }
        e->vx*=e->on_ground ? .588f : .98f;e->vz*=e->on_ground ? .588f : .98f;e->vy*=.98f;
        if(e->on_ground){e->vy*= -.5f;
            if(world_peek_block(w,(int)floorf(e->x),(int)floorf(e->y-.126f),(int)floorf(e->z))==79){e->vx*=.9604f/.588f;e->vz*=.9604f/.588f;}
        }
    }
}
void entity_spider_leg_pose(float phase,float amount,int leg,float *yaw,float *roll)
{
    static const float base_y[4]={.78539816f,.39269908f,-.39269908f,-.78539816f};
    static const float offsets[4]={0,3.14159265f,1.57079633f,4.71238898f};
    int pair=(leg&7)/2;float sign=(leg&1) ? -1 : 1;
    *yaw=sign*(base_y[pair]-cosf(phase*.6662f*2+offsets[pair])*.4f*amount);
    *roll=sign*(-.78539816f*((pair==1||pair==2) ? .74f : 1)+fabsf(sinf(phase*.6662f+offsets[pair]))*.4f*amount);
}
int entity_pick_except(const RenderEntity *entities,int count,const RendererCamera *camera,float reach,float block_distance,int ignored)
{
    float dir[3]={sinf(camera->yaw)*cosf(camera->pitch),sinf(camera->pitch),-cosf(camera->yaw)*cosf(camera->pitch)};
    float origin[3]={camera->x,camera->y,camera->z},nearest=fminf(reach,block_distance);int i,result=-1;
    for(i=0;i<count;++i) {
        const RenderEntity *e=&entities[i];float width=.6f,height=1.8f,lo[3],hi[3],enter=0,leave=nearest;int axis;
        if(!e->active || e->id==ignored || e->death || (e->type!=0 && e->type!=50 && e->type!=51 && e->type!=52 && e->type!=54 &&
            e->type!=90 && e->type!=91 && e->type!=92 && e->type!=93 && e->type!=1001 && e->type!=1002))continue;
        if(e->type==52){width=1.4f;height=.9f;}else if(e->type==90){width=.9f;height=.9f;}
        else if(e->type==91||e->type==92){width=.9f;height=1.3f;}else if(e->type==93){width=.3f;height=.4f;}
        else if(e->type==1001){width=.98f;height=.7f;}else if(e->type==1002){width=1.5f;height=.6f;}
        lo[0]=e->x-width*.5f-.1f;lo[1]=e->y-.1f;lo[2]=e->z-width*.5f-.1f;
        hi[0]=e->x+width*.5f+.1f;hi[1]=e->y+height+.1f;hi[2]=e->z+width*.5f+.1f;
        if(e->type==1001){lo[1]-=.35f;hi[1]-=.35f;}else if(e->type==1002){lo[1]-=.3f;hi[1]-=.3f;}
        for(axis=0;axis<3;++axis) {
            if(fabsf(dir[axis])<1e-6f){if(origin[axis]<lo[axis]||origin[axis]>hi[axis])break;}
            else {float a=(lo[axis]-origin[axis])/dir[axis],b=(hi[axis]-origin[axis])/dir[axis];
                if(a>b){float t=a;a=b;b=t;}enter=fmaxf(enter,a);leave=fminf(leave,b);if(enter>leave)break;}
        }
        if(axis==3 && enter<nearest){nearest=enter;result=e->id;}
    }
    return result;
}
int entity_pick(const RenderEntity *e,int count,const RendererCamera *camera,float reach,float block_distance)
{ return entity_pick_except(e,count,camera,reach,block_distance,-1); }
