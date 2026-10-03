#include "entity_render.h"
#include "../assets/assets.h"
#include "../world/beta_blocks.h"
#include "creative.h"
#include "player.h"
#include "skin_geometry.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <math.h>

static void box(float x, float y, float z, float half, float height)
{
    float a=x-half,b=x+half,c=z-half,d=z+half,e=y+height;
    glVertex3f(a,y,c); glVertex3f(b,y,c); glVertex3f(b,e,c); glVertex3f(a,e,c);
    glVertex3f(b,y,d); glVertex3f(a,y,d); glVertex3f(a,e,d); glVertex3f(b,e,d);
    glVertex3f(a,y,d); glVertex3f(a,y,c); glVertex3f(a,e,c); glVertex3f(a,e,d);
    glVertex3f(b,y,c); glVertex3f(b,y,d); glVertex3f(b,e,d); glVertex3f(b,e,c);
    glVertex3f(a,e,c); glVertex3f(b,e,c); glVertex3f(b,e,d); glVertex3f(a,e,d);
    glVertex3f(a,y,d); glVertex3f(b,y,d); glVertex3f(b,y,c); glVertex3f(a,y,c);
}

static GLint scene_begin(const RendererCamera *camera,int width,int height,float far_plane)
{
    GLint mode; float top=.05f*tanf(camera->fov_y*.00872664625997f),aspect=(float)width/height;
    glGetIntegerv(GL_MATRIX_MODE,&mode); glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glFrustum(-top*aspect,top*aspect,-top,top,.05,far_plane);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glRotatef(-camera->pitch*57.2957795131f,1,0,0); glRotatef(camera->yaw*57.2957795131f,0,1,0);
    glTranslatef(-camera->x,-camera->y,-camera->z); glViewport(0,0,width,height);
    glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D); glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,.1f);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    return mode;
}
static void scene_end(GLint mode)
{ glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
  glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(mode); glPopAttrib(); }

/* The six face rectangles follow Beta ModelRenderer's 64x32 skin layout. */
int entity_pick(const RenderEntity *entities,int count,const RendererCamera *camera,float reach,float block_distance)
{
    float direction[3]={sinf(camera->yaw)*cosf(camera->pitch),sinf(camera->pitch),-cosf(camera->yaw)*cosf(camera->pitch)};
    float origin[3]={camera->x,camera->y,camera->z},nearest=fminf(reach,block_distance);
    int i,result=-1;
    for(i=0;i<count;++i) {
        const RenderEntity *e=&entities[i]; int axis;
        float width=.6f,height=1.8f,lo[3],hi[3],enter=0,leave=nearest;
        if(!e->active || (e->type!=0 && e->type!=50 && e->type!=51 && e->type!=52 && e->type!=54 &&
           e->type!=90 && e->type!=91 && e->type!=92 && e->type!=93 && e->type!=1001 && e->type!=1002)) continue;
        if(e->type==52) { width=1.4f; height=.9f; }
        else if(e->type==90) { width=.9f; height=.9f; }
        else if(e->type==91 || e->type==92) { width=.9f; height=1.3f; }
        else if(e->type==93) { width=.3f; height=.4f; }
        else if(e->type==1001) { width=.98f; height=.7f; }
        else if(e->type==1002) { width=1.5f; height=.6f; }
        lo[0]=e->x-width*.5f-.1f; lo[1]=e->y-.1f; lo[2]=e->z-width*.5f-.1f;
        hi[0]=e->x+width*.5f+.1f; hi[1]=e->y+height+.1f; hi[2]=e->z+width*.5f+.1f;
        if(e->type==1002) { lo[1]-=.3f; hi[1]-=.3f; }
        for(axis=0;axis<3;++axis) {
            if(fabsf(direction[axis])<1e-6f) { if(origin[axis]<lo[axis] || origin[axis]>hi[axis]) break; }
            else {
                float a=(lo[axis]-origin[axis])/direction[axis],b=(hi[axis]-origin[axis])/direction[axis];
                if(a>b) { float swap=a; a=b; b=swap; }
                enter=fmaxf(enter,a); leave=fminf(leave,b); if(enter>leave) break;
            }
        }
        if(axis==3 && enter<nearest) { nearest=enter; result=e->id; }
    }
    return result;
}

static int skin_height=32;
static int player_modern;
static void held_cube(const InventorySlot *item);
static void skin_box_inflated(float x,float y,float z,int w,int h,int d,int u,int v,int mirror,float inflate)
{
    static const float normal[6][3]={{1,0,0},{-1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    int f,i; glBegin(GL_QUADS);
    for(f=0;f<6;++f) {
        glNormal3f(normal[f][0]*(mirror?-1:1),normal[f][1],normal[f][2]);
        for(i=0;i<4;++i) {
            float p[3],uv[2];
            skin_vertex(x,y,z,w,h,d,u,v,mirror,skin_height,inflate,f,i,p,uv);
            glTexCoord2fv(uv); glVertex3fv(p);
        }
    }
    glEnd();
}
static void skin_box(float x,float y,float z,int w,int h,int d,int u,int v,int mirror)
{ skin_box_inflated(x,y,z,w,h,d,u,v,mirror,0); }
static void limb(float x,float y,float angle,int arm,int mirror,int thin)
{
    int modern=player_modern&&mirror,u=arm?40:0,v=16;
    if(modern) {u=arm?32:16;v=48;}
    glPushMatrix(); glTranslatef(x,y,0); glRotatef(angle,1,0,0);
    skin_box(thin?-1:arm?(mirror?-1:-3):-2,arm?-2:0,thin?-1:-2,thin?2:4,12,thin?2:4,u,v,modern?0:mirror);
    if(player_modern) {
        int outer_u=mirror?(arm?48:0):(arm?40:0),outer_v=mirror?48:32;
        skin_box_inflated(arm?(mirror?-1:-3):-2,arm?-2:0,-2,4,12,4,outer_u,outer_v,0,.25f);
    }
    glPopMatrix();
}
static void player_model_pose(const RenderEntity *e,float head_yaw)
{
    float angle=sinf(e->walk)*32;
    int previous_height=skin_height,previous_modern=player_modern;
    if(e->type==0) skin_height=assets_get_texture(ASSET_PLAYER_SKIN).height;
    player_modern=e->type==0&&skin_height==64;
    glPushMatrix(); glTranslatef(e->draw_x,e->draw_y+1.40625f,e->draw_z);
    glRotatef(180-e->yaw,0,1,0); glScalef(.05859375f,-.05859375f,.05859375f);
    glColor3ub(255,255,255);
    skin_box(-4,0,-2,8,12,4,16,16,0);
    if(player_modern) skin_box_inflated(-4,0,-2,8,12,4,16,32,0,.25f);
    glPushMatrix(); glRotatef(head_yaw,0,1,0); glRotatef(e->pitch,1,0,0);
    skin_box(-4,-8,-4,8,8,8,0,0,0);
    if(e->type==0) skin_box_inflated(-4,-8,-4,8,8,8,32,0,0,.5f);
    glPopMatrix();
    limb(-5,2,e->type==54 || e->type==51 ? -90+e->pitch : angle,1,0,e->type==51);
    limb(5,2,e->type==54 || e->type==51 ? -90+e->pitch : -angle,1,1,e->type==51);
    limb(-2,12,-angle,0,0,e->type==51); limb(2,12,angle,0,1,e->type==51); glPopMatrix();
    skin_height=previous_height;player_modern=previous_modern;
}
static void player_model(const RenderEntity *e) { player_model_pose(e,0); }

void player_inventory_draw(int x,int feet_y,int scale,float mouse_x,float mouse_y,int width,int height)
{
    RenderEntity preview={0}; GLint mode; Texture2D skin;
    float units,yaw,pitch;
    const GLfloat light0[4]={-.2f,.8f,.6f,0},light1[4]={.2f,.8f,-.6f,0};
    const GLfloat diffuse[4]={.6f,.6f,.6f,1},ambient[4]={.4f,.4f,.4f,1},none[4]={0,0,0,1};
    int left,bottom,clip_width,clip_height,i;
    if(scale<=0 || width<=0 || height<=0) return;
    units=(float)scale/30;
    yaw=atanf((x-mouse_x)/(40*units))*20;
    pitch=-atanf((feet_y-50*units-mouse_y)/(40*units))*20;
    preview.yaw=yaw; preview.pitch=pitch;
    glGetIntegerv(GL_MATRIX_MODE,&mode); glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0,width,height,0,-500,500);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glViewport(0,0,width,height); glDisable(GL_FOG); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthRange(0,1); glDepthMask(GL_TRUE);
    /* Clear only the model window's depth, so terrain near the camera cannot
     * occlude the GUI model. Color/borders and other UI regions stay intact. */
    left=(int)floorf(x-25*units); bottom=(int)floorf(height-feet_y-3*units);
    clip_width=(int)ceilf(50*units); clip_height=(int)ceilf(70*units);
    glEnable(GL_SCISSOR_TEST); glScissor(left,bottom,clip_width,clip_height);
    glClearDepth(1); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D); glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,.1f);
    skin=assets_get_texture(ASSET_PLAYER_SKIN); glBindTexture(GL_TEXTURE_2D,skin.id);
    for(i=0;i<8;++i) glDisable(GL_LIGHT0+i);
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1); glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    glLightfv(GL_LIGHT0,GL_POSITION,light0); glLightfv(GL_LIGHT1,GL_POSITION,light1);
    glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse); glLightfv(GL_LIGHT1,GL_DIFFUSE,diffuse);
    glLightfv(GL_LIGHT0,GL_AMBIENT,none); glLightfv(GL_LIGHT1,GL_AMBIENT,none);
    glLightfv(GL_LIGHT0,GL_SPECULAR,none); glLightfv(GL_LIGHT1,GL_SPECULAR,none);
    glTranslatef((float)x,(float)feet_y,50); glScalef((float)scale,(float)-scale,(float)scale);
    glRotatef(pitch,1,0,0); player_model_pose(&preview,-yaw);
    glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glPopAttrib(); glMatrixMode(mode);
}
static void model_part(float x,float y,float z,float rotation,int bx,int by,int bz,int w,int h,int d,int u,int v)
{
    glPushMatrix(); glTranslatef(x,y,z); glRotatef(rotation,1,0,0);
    skin_box((float)bx,(float)by,(float)bz,w,h,d,u,v,0); glPopMatrix();
}
static AssetId mob_skin(int type)
{
    switch(type) {
    case 50: return ASSET_MOB_CREEPER; case 51: return ASSET_MOB_SKELETON;
    case 52: return ASSET_MOB_SPIDER; case 54: return ASSET_MOB_ZOMBIE;
    case 90: return ASSET_MOB_PIG; case 91: return ASSET_MOB_SHEEP;
    case 92: return ASSET_MOB_COW; case 93: return ASSET_MOB_CHICKEN;
    default: return ASSET_PLAYER_SKIN;
    }
}
static void mob_model(const RenderEntity *e)
{
    float angle=sinf(e->walk)*32; int leg,i;
    glColor3ub(255,255,255);
    if(e->type==51 || e->type==54) { player_model(e); return; }
    glPushMatrix(); glTranslatef(e->draw_x,e->draw_y+(e->type==50 ? 1.375f : 1.5f),e->draw_z);
    if(e->type==50 && e->fuse>0) {
        float phase=fminf(1,(e->fuse+e->phase)/30),pulse=1+sinf(phase*100)*phase*.01f;
        phase*=phase;phase*=phase;glScalef((1+phase*.4f)*pulse,(1+phase*.1f)/pulse,(1+phase*.4f)*pulse);
    }
    glRotatef(180-e->yaw,0,1,0); glScalef(.0625f,-.0625f,.0625f);
    if(e->type==90 || e->type==91 || e->type==92) {
        leg=e->type==90 ? 6 : 12;
        model_part(0,(float)(18-leg),-6,0,-4,-4,-8,8,8,8,0,0);
        model_part(0,(float)(17-leg),2,90,e->type==92 ? -6 : -5,-10,-7,
            e->type==92 ? 12 : 10,e->type==92 ? 18 : 16,e->type==92 ? 10 : 8,e->type==92 ? 18 : 28,e->type==92 ? 4 : 8);
        for(i=0;i<4;++i) model_part(i&1 ? 3 : -3,(float)(24-leg),i<2 ? 7 : -5,
            ((i==0 || i==3) ? angle : -angle),-2,0,-2,4,leg,4,0,16);
        if(e->type==91 && !e->sheared) {
            static const float colors[16][3]={{1,1,1},{.95f,.7f,.2f},{.9f,.5f,.85f},{.6f,.7f,.95f},
                {.9f,.9f,.2f},{.5f,.8f,.1f},{.95f,.5f,.65f},{.3f,.3f,.3f},{.6f,.6f,.6f},{.3f,.5f,.6f},
                {.5f,.25f,.7f},{.2f,.3f,.7f},{.4f,.3f,.2f},{.4f,.5f,.2f},{.6f,.2f,.2f},{.1f,.1f,.1f}};
            glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_MOB_SHEEP_FUR).id);
            glColor3fv(colors[e->color&15]);
            model_part(0,6,-8,0,-3,-4,-6,6,6,6,0,0);
            model_part(0,5,2,90,-4,-10,-7,8,16,6,28,8);
        }
    } else if(e->type==93) {
        model_part(0,15,-4,0,-2,-6,-2,4,6,3,0,0);
        model_part(0,15,-4,0,-2,-4,-4,4,2,2,14,0);
        model_part(0,15,-4,0,-1,-2,-3,2,2,2,14,4);
        model_part(0,16,0,90,-3,-4,-3,6,8,6,0,9);
        model_part(-2,19,1,angle,-1,0,-3,3,5,3,26,0);
        model_part(1,19,1,-angle,-1,0,-3,3,5,3,26,0);
        model_part(-4,13,0,0,0,0,-3,1,4,6,24,13);
        model_part(4,13,0,0,-1,0,-3,1,4,6,24,13);
    } else if(e->type==50) {
        model_part(0,4,0,0,-4,-8,-4,8,8,8,0,0);
        model_part(0,4,0,0,-4,0,-2,8,12,4,16,16);
        for(i=0;i<4;++i) model_part(i&1 ? 2 : -2,16,i<2 ? 4 : -4,
            (i==0 || i==3) ? angle : -angle,-2,0,-2,4,6,4,0,16);
    } else if(e->type==52) {
        model_part(0,15,-3,0,-4,-4,-8,8,8,8,32,4);
        model_part(0,15,0,0,-3,-3,-3,6,6,6,0,0);
        model_part(0,15,9,0,-5,-4,-6,10,8,12,0,12);
        for(i=0;i<8;++i) {
            glPushMatrix(); glTranslatef(i&1 ? 4 : -4,15,(float)(2-i/2));
            glRotatef((i&1 ? -1 : 1)*(30+sinf(e->walk+(float)i)*10),0,0,1);
            glRotatef((float)(i/2-2)*20,0,1,0);
            skin_box(i&1 ? -1 : -15,-1,-1,16,2,2,18,0,0); glPopMatrix();
        }
    }
    glPopMatrix();
}
static void held_cube(const InventorySlot *item);
static void transport_model(const RenderEntity *e)
{
    int n; glPushMatrix(); glTranslatef(e->draw_x,e->draw_y,e->draw_z);
    glEnable(GL_TEXTURE_2D); glColor3ub(255,255,255);
    if(e->type==1000) {
        glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_ARROW).id);
        glRotatef(90-e->yaw,0,1,0); glRotatef(e->pitch,0,0,1); glRotatef(45,1,0,0);
        glScalef(.05625f,.05625f,.05625f); glTranslatef(-4,0,0);
        glDisable(GL_CULL_FACE);
        glBegin(GL_QUADS);
        glTexCoord2f(0,5.0f/32); glVertex3f(-7,-2,-2);
        glTexCoord2f(5.0f/32,5.0f/32); glVertex3f(-7,-2,2);
        glTexCoord2f(5.0f/32,10.0f/32); glVertex3f(-7,2,2);
        glTexCoord2f(0,10.0f/32); glVertex3f(-7,2,-2); glEnd();
        for(n=0;n<4;++n) {
            glRotatef(90,1,0,0); glBegin(GL_QUADS);
            glTexCoord2f(0,0); glVertex3f(-8,-2,0); glTexCoord2f(.5f,0); glVertex3f(8,-2,0);
            glTexCoord2f(.5f,5.0f/32); glVertex3f(8,2,0); glTexCoord2f(0,5.0f/32); glVertex3f(-8,2,0); glEnd();
        }
    } else if(e->type==1002) {
        glRotatef(180-e->yaw,0,1,0); glScalef(-.0625f,-.0625f,.0625f);
        glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_BOAT).id);
        glPushMatrix(); glTranslatef(0,4,0); glRotatef(90,1,0,0); skin_box(-12,-8,-3,24,16,4,0,8,0); glPopMatrix();
        for(n=0;n<4;++n) {
            glPushMatrix(); glTranslatef(n<2 ? (n==0 ? -11 : 11) : 0,4,n>=2 ? (n==2 ? -9 : 9) : 0);
            glRotatef(n==0 ? 270 : n==1 ? 90 : n==2 ? 180 : 0,0,1,0);
            skin_box(-10,-7,-1,20,6,2,0,0,0); glPopMatrix();
        }
    } else {
        glRotatef(180-e->yaw,0,1,0); glRotatef(-e->pitch,0,0,1);
        if(e->color==1 || e->color==2) {
            InventorySlot contents={e->color==1 ? 54 : 61,1,0};
            glPushMatrix(); glScalef(.75f,.75f,.75f); glTranslatef(0,.3125f,0); glRotatef(90,0,1,0);
            held_cube(&contents); glPopMatrix();
        }
        glScalef(-.0625f,-.0625f,.0625f);
        glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_MINECART).id);
        glPushMatrix(); glTranslatef(0,4,0); glRotatef(90,1,0,0); skin_box(-10,-8,-1,20,16,2,0,10,0); glPopMatrix();
        for(n=0;n<4;++n) {
            glPushMatrix(); glTranslatef(n<2 ? (n==0 ? -9 : 9) : 0,4,n>=2 ? (n==2 ? -7 : 7) : 0);
            glRotatef(n==0 ? 270 : n==1 ? 90 : n==2 ? 180 : 0,0,1,0);
            skin_box(-8,-9,-1,16,8,2,0,0,0); glPopMatrix();
        }
        glPushMatrix(); glTranslatef(0,4,0); glRotatef(-90,1,0,0); skin_box(-9,-7,-1,18,14,1,44,10,0); glPopMatrix();
    }
    glPopMatrix();
}
int entity_render_draw(RenderEntity *entities,int count,const RendererCamera *camera,
                       int width,int height,int distance,float dt)
{
    GLint mode; int i,rendered=0; float far_plane=(float)(distance*16+32),far2=far_plane*far_plane;
    Texture2D skin;
    if (!entities || count<=0 || !camera || width<=0 || height<=0) return 0;
    /* No GL state queries or texture upload when no tracked entities exist. */
    for (i=0;i<count && !entities[i].active;++i) { }
    if (i==count) return 0;
    skin=assets_get_texture(ASSET_PLAYER_SKIN); mode=scene_begin(camera,width,height,far_plane);
    for (i=0;i<count && rendered<64;++i) {
        RenderEntity *e=&entities[i]; float dx,dy,dz,a=dt*10,move;
        if (!e->active) continue;
        dx=e->x-e->draw_x; dy=e->y-e->draw_y; dz=e->z-e->draw_z;
        if(e->local_interpolation) {
            e->phase+=dt*20; if(e->phase>1) e->phase=1;
            e->draw_x=e->previous_x+(e->x-e->previous_x)*e->phase;
            e->draw_y=e->previous_y+(e->y-e->previous_y)*e->phase;
            e->draw_z=e->previous_z+(e->z-e->previous_z)*e->phase; e->positioned=1;
        } else if (!e->positioned || dx*dx+dy*dy+dz*dz>64) { e->draw_x=e->x; e->draw_y=e->y; e->draw_z=e->z; e->positioned=1; }
        else {
            if(a>1) a=1;
            if(a<0) a=0;
            move=sqrtf(dx*dx+dz*dz)*a;
            e->draw_x+=dx*a; e->draw_y+=dy*a; e->draw_z+=dz*a; e->walk+=move*6;
        }
        dx=e->draw_x-camera->x; dy=e->draw_y-camera->y; dz=e->draw_z-camera->z;
        if(dx*dx+dy*dy+dz*dz>far2) continue;
        if(e->type==1003) {
            float swell=1;
            InventorySlot tnt={46,1,0};glPushMatrix();glTranslatef(e->draw_x,e->draw_y,e->draw_z);
            if(e->fuse<10){float phase=1-(e->fuse-e->phase+1)/10;if(phase<0)phase=0;phase*=phase;phase*=phase;swell+=phase*.3f;}
            glScalef(.98f*swell,.98f*swell,.98f*swell);held_cube(&tnt);
            if((e->fuse/5)%2==0) {
                glDisable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glColor4f(1,1,1,(1-(e->fuse-e->phase+1)/100)*.8f);
                glBegin(GL_QUADS);box(0,-.5f,0,.5f,1);glEnd();
                glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_TEXTURE_2D);
            }
            glPopMatrix();
        }
        else if(e->type==1000 || e->type==1001 || e->type==1002) transport_model(e);
        else if(e->type==0) { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,skin.id); player_model(e); }
        else if(e->type==50 || e->type==51 || e->type==52 || e->type==54 || (e->type>=90 && e->type<=93)) {
            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,assets_get_texture(mob_skin(e->type)).id); mob_model(e);
        }
        else {
            glDisable(GL_TEXTURE_2D); glColor3ub(84,133,72); glBegin(GL_QUADS);
            box(e->draw_x,e->draw_y,e->draw_z,.27f,1.25f); box(e->draw_x,e->draw_y+1.25f,e->draw_z,.22f,.48f);
            glEnd();
        }
        ++rendered;
    }
    scene_end(mode); return rendered;
}

static void held_cube(const InventorySlot *item);
static void held_sprite(int tile,int block);

int item_drop_draw(const ItemDrop *drops,int count,const RendererCamera *camera,
                   int width,int height,int render_distance_chunks)
{
    GLint mode;
    float far_plane=(float)(render_distance_chunks*WORLD_CHUNK_SIZE+32),far2=far_plane*far_plane;
    int i,drawn=0;
    if (!drops || !camera || count<=0 || width<=0 || height<=0) return 0;
    for(i=0;i<count && !drops[i].active;++i) { }
    if(i==count) return 0;
    mode=scene_begin(camera,width,height,far_plane);
    for (i=0;i<count;++i) if (drops[i].active) {
        const ItemDrop *d=&drops[i];
        InventorySlot item={d->id,d->count,d->damage};
        float dx=d->x-camera->x,dy=d->y-camera->y,dz=d->z-camera->z;
        int n,copies=d->count>20 ? 4 : d->count>5 ? 3 : d->count>1 ? 2 : 1;
        int block=d->id>0 && d->id<BETA_BLOCK_COUNT;
        int cube=block && world_block_def((uint8_t)d->id)->solid;
        if (dx*dx+dy*dy+dz*dz>far2) continue;
        glPushMatrix();
        glTranslatef(d->x,d->y+.15f+sinf(d->age*2)*.05f,d->z);
        glRotatef(cube ? d->age*57.2957795f : -camera->yaw*57.2957795f,0,1,0);
        glScalef(cube ? .25f : .5f,cube ? .25f : .5f,cube ? .25f : .5f);
        for(n=0;n<copies;++n) {
            glPushMatrix();
            if(n) glTranslatef((n&1 ? .13f : -.13f),n*.06f,(n&2 ? .13f : -.13f));
            if(cube) held_cube(&item);
            else {
                int tile=block ? beta_block_terrain_tile((BetaBlockState){(uint8_t)d->id,(uint8_t)d->damage},2) : beta_item_tile(d->id,d->damage);
                if(tile>=0) held_sprite(tile,block);
            }
            glPopMatrix();
        }
        glPopMatrix(); ++drawn;
    }
    scene_end(mode); return drawn;
}

static void held_cube(const InventorySlot *item)
{
    static const int faces[6][4]={{4,5,1,0},{3,2,6,7},{1,5,6,2},{4,0,3,7},{0,1,2,3},{5,4,7,6}};
    static const float points[8][3]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
    int f,c; Texture2D terrain=assets_get_texture(ASSET_TERRAIN);
    glBindTexture(GL_TEXTURE_2D,terrain.id); glBegin(GL_QUADS);
    for (f=0;f<6;++f) {
        int tile=beta_block_terrain_tile((BetaBlockState){(uint8_t)item->id,(uint8_t)item->damage},(unsigned)f);
        float u0,u1,v0,v1; if(tile<0) tile=1;
        u0=((tile%16)*16+.01f)/256; u1=((tile%16)*16+15.99f)/256;
        v0=((tile/16)*16+.01f)/256; v1=((tile/16)*16+15.99f)/256;
        if (tile==0 || tile==52 || tile==132) glColor3ub(116,174,73); else glColor3ub(255,255,255);
        for(c=0;c<4;++c) {
            glTexCoord2f(c==0 || c==3 ? u0 : u1,c<2 ? v1 : v0); glVertex3fv(points[faces[f][c]]);
        }
    }
    glEnd();
}
static void held_sprite(int tile,int block)
{
    Texture2D image=assets_get_texture(block ? ASSET_TERRAIN : ASSET_GUI_ITEMS);
    float u0=((tile%16)*16+.01f)/256,u1=((tile%16)*16+15.99f)/256;
    float v0=((tile/16)*16+.01f)/256,v1=((tile/16)*16+15.99f)/256;
    glBindTexture(GL_TEXTURE_2D,image.id); glColor3ub(255,255,255);
    glBegin(GL_QUADS);
    glTexCoord2f(u0,v1); glVertex3f(-.5f,-.5f,0); glTexCoord2f(u1,v1); glVertex3f(.5f,-.5f,0);
    glTexCoord2f(u1,v0); glVertex3f(.5f,.5f,0); glTexCoord2f(u0,v0); glVertex3f(-.5f,.5f,0);
    glEnd();
}
static void held_extruded_sprite(int tile,int block)
{
    Texture2D image=assets_get_texture(block ? ASSET_TERRAIN : ASSET_GUI_ITEMS);
    float u0=(tile%16)*16/256.0f,u1=((tile%16)*16+15.99f)/256;
    float v0=(tile/16)*16/256.0f,v1=((tile/16)*16+15.99f)/256;
    int n;
    glBindTexture(GL_TEXTURE_2D,image.id); glColor3ub(255,255,255); glBegin(GL_QUADS);
    glTexCoord2f(u1,v1); glVertex3f(-.5f,-.5f,0); glTexCoord2f(u0,v1); glVertex3f(.5f,-.5f,0);
    glTexCoord2f(u0,v0); glVertex3f(.5f,.5f,0); glTexCoord2f(u1,v0); glVertex3f(-.5f,.5f,0);
    glTexCoord2f(u1,v0); glVertex3f(-.5f,.5f,-.0625f); glTexCoord2f(u0,v0); glVertex3f(.5f,.5f,-.0625f);
    glTexCoord2f(u0,v1); glVertex3f(.5f,-.5f,-.0625f); glTexCoord2f(u1,v1); glVertex3f(-.5f,-.5f,-.0625f);
    /* ItemRenderer Beta extrudes all sixteen columns and rows. Alpha test
     * removes transparent samples; this also closes holes inside a sprite. */
    for(n=0;n<16;++n) {
        float t=n/16.0f,u=u1+(u0-u1)*t-.5f/256,v=v1+(v0-v1)*t-.5f/256;
        float x=t-.5f,y=t-.5f; int side;
        for(side=0;side<2;++side) {
            float sx=x+side*.0625f,sy=y+side*.0625f;
            glTexCoord2f(u,v1); glVertex3f(sx,-.5f,-.0625f); glTexCoord2f(u,v1); glVertex3f(sx,-.5f,0);
            glTexCoord2f(u,v0); glVertex3f(sx,.5f,0); glTexCoord2f(u,v0); glVertex3f(sx,.5f,-.0625f);
            glTexCoord2f(u1,v); glVertex3f(-.5f,sy,0); glTexCoord2f(u0,v); glVertex3f(.5f,sy,0);
            glTexCoord2f(u0,v); glVertex3f(.5f,sy,-.0625f); glTexCoord2f(u1,v); glVertex3f(-.5f,sy,-.0625f);
        }
    }
    glEnd();
}
void first_person_tick(FirstPersonState *s,const InventorySlot *item,int slot)
{
    int same=s->item.id==item->id && s->item.damage==item->damage && s->slot==slot;
    float d=(same ? 1.0f : 0)-s->equip;
    s->previous_equip=s->equip;
    if(d>.4f) d=.4f;
    if(d<-.4f) d=-.4f;
    s->equip+=d;
    if(s->equip<.1f) { s->item=*item; s->slot=slot; }
    else if(same) s->item=*item;
}
void first_person_draw(const InventorySlot *item,int width,int height,float swing,int hurt)
{ first_person_draw_pose(item,width,height,swing,hurt,1,0); }
void first_person_draw_pose(const InventorySlot *item,int width,int height,float swing,int hurt,float equip,float bob)
{
    RendererCamera camera={0}; GLint mode; float arc,phase=sinf(swing*3.14159265f);
    if(width<=0 || height<=0) return;
    camera.fov_y=70; mode=scene_begin(&camera,width,height,10);
    glDepthRange(0,.1); /* Keep the hand close in depth without clearing terrain. */
    if (swing<0) swing=0;
    if(swing>1) swing=1;
    arc=sinf(sqrtf(swing)*3.14159265f);
    glTranslatef(0,bob,0);
    if(hurt>0) glRotatef(sinf(hurt*.3f)*8,0,0,1);
    if(item && item->id>0 && item->count>0) {
        /* Beta ItemRenderer: every held item swings about the same hand pivot.
         * Inventory 3D blocks and extruded sprites then use distinct poses. */
        glTranslatef(-arc*.4f,sinf(sqrtf(swing)*6.2831853f)*.2f,-phase*.2f);
        glTranslatef(.56f,-.52f-(1-equip)*.6f,-.72f); glRotatef(45,0,1,0);
        glRotatef(-sinf(swing*swing*3.14159265f)*20,0,1,0);
        glRotatef(-arc*20,0,0,1); glRotatef(-arc*80,1,0,0); glScalef(.4f,.4f,.4f);
        if(item->id<97 && !beta_block_cross_plant(item->id) && item->id!=50 && item->id!=75 && item->id!=76 && item->id!=55 && item->id!=69 && item->id!=77 && item->id!=66 && item->id!=27 && item->id!=28 && item->id!=63 && item->id!=68) {
            held_cube(item);
        } else {
            int tile=item->id<97 ? beta_block_terrain_tile((BetaBlockState){(uint8_t)item->id,(uint8_t)item->damage},2) : beta_item_tile(item->id,item->damage);
            if(tile>=0) {
                if(item->id==346) glRotatef(180,0,1,0); /* ItemFishingRod.shouldRotate... */
                glTranslatef(0,-.3f,0); glScalef(1.5f,1.5f,1.5f);
                glRotatef(50,0,1,0); glRotatef(335,0,0,1); glTranslatef(-.4375f,.4375f,0);
                held_extruded_sprite(tile,item->id<97);
            }
        }
    } else {
        glTranslatef(-arc*.3f,sinf(sqrtf(swing)*6.2831853f)*.4f,-phase*.4f);
        glTranslatef(.64f,-.6f-(1-equip)*.6f,-.72f); glRotatef(45,0,1,0);
        glRotatef(arc*70,0,1,0); glRotatef(-sinf(swing*swing*3.14159265f)*20,0,0,1);
        glTranslatef(-1,3.6f,3.5f); glRotatef(120,0,0,1); glRotatef(200,1,0,0); glRotatef(-135,0,1,0);
        glTranslatef(5.6f,0,0); glScalef(.0625f,.0625f,.0625f);
        Texture2D skin=assets_get_texture(ASSET_PLAYER_SKIN);int previous_height=skin_height;
        glBindTexture(GL_TEXTURE_2D,skin.id);skin_height=skin.height;
        glColor3ub(255,255,255); skin_box(-8,0,-2,4,12,4,40,16,0);
        if(skin.height==64) skin_box_inflated(-8,0,-2,4,12,4,40,32,0,.25f);
        skin_height=previous_height;
    }
    scene_end(mode);
}
void mining_cracks_draw(const RendererCamera *camera,int width,int height,
                        int x,int y,int z,BetaBlockState state,float progress)
{
    GLint mode; BetaBlockBox b; int stage=(int)(progress*10),f;
    float u0,u1,v0,v1;
    static const int faces[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
    float p[8][3];
    if(progress<=0 || width<=0 || height<=0) return;
    if(stage>9) stage=9;
    if(!beta_block_selection_box(state,&b)) { b.min_x=b.min_y=b.min_z=0; b.max_x=b.max_y=b.max_z=1; }
    for(f=0;f<8;++f) {
        p[f][0]=x+((f==1 || f==2 || f==5 || f==6) ? b.max_x : b.min_x);
        p[f][1]=y+((f==2 || f==3 || f==6 || f==7) ? b.max_y : b.min_y);
        p[f][2]=z+(f>=4 ? b.max_z : b.min_z);
    }
    u0=(stage*16+.01f)/256; u1=(stage*16+15.99f)/256; v0=240.01f/256; v1=255.99f/256;
    mode=scene_begin(camera,width,height,256); glBindTexture(GL_TEXTURE_2D,assets_get_texture(ASSET_TERRAIN).id);
    glEnable(GL_BLEND); glBlendFunc(GL_DST_COLOR,GL_SRC_COLOR); glColor4f(1,1,1,.5f);
    glDepthMask(GL_FALSE); glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(-1,-1); glBegin(GL_QUADS);
    for(f=0;f<6;++f) { int i; for(i=0;i<4;++i) { glTexCoord2f(i==0 || i==3 ? u0 : u1,i<2 ? v1 : v0); glVertex3fv(p[faces[f][i]]); } }
    glEnd(); scene_end(mode);
}
