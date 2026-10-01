#include "entity_render.h"
#include "../assets/assets.h"
#include "../world/beta_blocks.h"

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

int entity_render_draw(const RenderEntity *entities, int count,
                       const RendererCamera *camera, int width, int height,
                       int render_distance_chunks)
{
    GLint old_mode;
    float fov, top, aspect, far_plane, sy, cy, sp, cp, far2;
    int i, rendered=0;
    if (!entities || count<=0 || !camera || width<=0 || height<=0) return 0;
    fov=camera->fov_y;
    if (fov<30) fov=30;
    if (fov>110) fov=110;
    aspect=(float)width/height;
    top=0.05f*tanf(fov*0.00872664625997f);
    far_plane=(float)(render_distance_chunks*WORLD_CHUNK_SIZE+32);
    far2=far_plane*far_plane;
    sy=sinf(camera->yaw); cy=cosf(camera->yaw);
    sp=sinf(camera->pitch); cp=cosf(camera->pitch);
    glGetIntegerv(GL_MATRIX_MODE,&old_mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glFrustum(-top*aspect,top*aspect,-top,top,0.05,far_plane);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glRotatef(-camera->pitch*57.2957795131f,1,0,0);
    glRotatef(camera->yaw*57.2957795131f,0,1,0);
    glTranslatef(-camera->x,-camera->y,-camera->z);
    glViewport(0,0,width,height);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glBegin(GL_QUADS);
    for (i=0; i<count; ++i) {
        const RenderEntity *entity=&entities[i];
        float dx,dy,dz,forward;
        if (rendered >= 64) break;
        if (!entity->active) continue;
        dx=entity->x-camera->x; dy=entity->y-camera->y; dz=entity->z-camera->z;
        if (dx*dx+dy*dy+dz*dz>far2) continue;
        forward=dx*sy*cp+dy*sp-dz*cy*cp;
        if (forward < -2.0f) continue;
        if (entity->type==0) glColor3ub(77,111,163);     /* player */
        else if (entity->type==50 || entity->type==54) glColor3ub(84,133,72);
        else glColor3ub(143,126,91);
        box(entity->x,entity->y,entity->z,0.27f,1.25f);
        glColor3ub(190,173,141);
        box(entity->x,entity->y+1.25f,entity->z,0.22f,0.48f);
        ++rendered;
    }
    glEnd();
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(old_mode); glPopAttrib();
    return rendered;
}

static int drop_tile(int id)
{
    BetaBlockState state;
    int tile;
    switch (id) {
    case BETA_BLOCK_STONE: return 1;
    case BETA_BLOCK_DIRT: return 2;
    case BETA_BLOCK_GRASS: return 0;
    case BETA_BLOCK_SAND: return 18;
    case BETA_BLOCK_COBBLESTONE: return 16;
    case BETA_BLOCK_LOG: return 20;
    case BETA_BLOCK_LEAVES: return 52;
    case BETA_BLOCK_GLASS: return 49;
    default: break;
    }
    state.id=(uint8_t)id; state.metadata=0;
    tile=beta_block_terrain_tile(state,2);
    return tile>=0 ? tile : 1;
}

static void drop_face(float u0,float v0,float u1,float v1,
                      const float p[4][3],unsigned char shade)
{
    glColor3ub(shade,shade,shade);
    glTexCoord2f(u0,v0); glVertex3fv(p[0]);
    glTexCoord2f(u1,v0); glVertex3fv(p[1]);
    glTexCoord2f(u1,v1); glVertex3fv(p[2]);
    glTexCoord2f(u0,v1); glVertex3fv(p[3]);
}

int item_drop_draw(const ItemDrop *drops,int count,const RendererCamera *camera,
                   int width,int height,int render_distance_chunks)
{
    static const float faces[6][4][3]={
        {{-.2f,0,-.2f},{.2f,0,-.2f},{.2f,.4f,-.2f},{-.2f,.4f,-.2f}},
        {{.2f,0,.2f},{-.2f,0,.2f},{-.2f,.4f,.2f},{.2f,.4f,.2f}},
        {{-.2f,0,.2f},{-.2f,0,-.2f},{-.2f,.4f,-.2f},{-.2f,.4f,.2f}},
        {{.2f,0,-.2f},{.2f,0,.2f},{.2f,.4f,.2f},{.2f,.4f,-.2f}},
        {{-.2f,.4f,-.2f},{.2f,.4f,-.2f},{.2f,.4f,.2f},{-.2f,.4f,.2f}},
        {{-.2f,0,.2f},{.2f,0,.2f},{.2f,0,-.2f},{-.2f,0,-.2f}}
    };
    Texture2D terrain;
    GLint old_mode;
    float top,aspect,far_plane,far2;
    int i,drawn=0;
    if (!drops || !camera || count<=0 || width<=0 || height<=0) return 0;
    terrain=assets_get_texture(ASSET_TERRAIN);
    top=0.05f*tanf(camera->fov_y*0.00872664625997f);
    aspect=(float)width/height;
    far_plane=(float)(render_distance_chunks*WORLD_CHUNK_SIZE+32);
    far2=far_plane*far_plane;
    glGetIntegerv(GL_MATRIX_MODE,&old_mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glFrustum(-top*aspect,top*aspect,-top,top,0.05,far_plane);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glRotatef(-camera->pitch*57.2957795131f,1,0,0);
    glRotatef(camera->yaw*57.2957795131f,0,1,0);
    glTranslatef(-camera->x,-camera->y,-camera->z);
    glViewport(0,0,width,height);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,terrain.id);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER,0.5f);
    glEnable(GL_DEPTH_TEST); glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    for (i=0;i<count;++i) if (drops[i].active) {
        float dx=drops[i].x-camera->x,dy=drops[i].y-camera->y,
              dz=drops[i].z-camera->z;
        int tile,face;
        float u0,u1,v0,v1;
        if (dx*dx+dy*dy+dz*dz>far2) continue;
        tile=drop_tile(drops[i].id);
        u0=((tile%16)*16+0.5f)/256.0f;
        u1=((tile%16)*16+15.5f)/256.0f;
        v0=((tile/16)*16+0.5f)/256.0f;
        v1=((tile/16)*16+15.5f)/256.0f;
        glPushMatrix();
        glTranslatef(drops[i].x,drops[i].y,drops[i].z);
        glRotatef(drops[i].age*85.0f,0,1,0);
        glBegin(GL_QUADS);
        for (face=0;face<6;++face)
            drop_face(u0,v0,u1,v1,faces[face],face==4 ? 255 : face==5 ? 140 : 195);
        glEnd();
        glPopMatrix();
        ++drawn;
    }
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(old_mode); glPopAttrib();
    return drawn;
}
