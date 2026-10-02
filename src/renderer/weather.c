#include "weather.h"
#include "../world/environment.h"
#include "../assets/assets.h"
#include <math.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
void renderer_weather(World *w,const RendererCamera *camera,int width,int height,int fancy,float partial)
{
    int radius=fancy ? 10 : 5,x,z,pass,cx=(int)floorf(camera->x),cz=(int)floorf(camera->z);
    float top=.05f*tanf(camera->fov_y*.00872664625997f),aspect;
    float time=(float)(w->beta_world_time%24000)+partial;
    GLint mode;
    if(w->rain_strength<=0 || width<=0 || height<=0) return;
    aspect=(float)width/height;
    glGetIntegerv(GL_MATRIX_MODE,&mode); glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glFrustum(-top*aspect,top*aspect,-top,top,.05,128);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glRotatef(-camera->pitch*57.2957795131f,1,0,0); glRotatef(camera->yaw*57.2957795131f,0,1,0);
    glTranslatef(-camera->x,-camera->y,-camera->z);
    glMatrixMode(GL_TEXTURE); glPushMatrix(); glLoadIdentity(); glMatrixMode(GL_MODELVIEW);
    glViewport(0,0,width,height); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_ALPHA_TEST); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
    for(pass=1;pass<=2;++pass) {
        Texture2D texture=assets_get_texture(pass==1 ? ASSET_RAIN : ASSET_SNOW);
        glBindTexture(GL_TEXTURE_2D,texture.id);
        glBegin(GL_QUADS);
        for(z=cz-radius;z<=cz+radius;++z) for(x=cx-radius;x<=cx+radius;++x) {
            unsigned kind; int roof=world_precipitation_height(w,x,z,&kind);
            uint32_t ux=(uint32_t)x,uz=(uint32_t)z;
            float low=camera->y-radius,high=camera->y+radius;
            float dx=x+.5f-camera->x,dz=z+.5f-camera->z,distance=sqrtf(dx*dx+dz*dz),sx,sz,alpha,scroll;
            if(kind!=(unsigned)pass || distance>radius || high<roof) continue;
            if(low<roof) low=(float)roof;
            if(distance<.01f) { sx=.5f; sz=0; }
            else { sx=-dz/distance*.5f; sz=dx/distance*.5f; }
            alpha=(1-distance*distance/(radius*radius))*(pass==1 ? .5f : .8f)*w->rain_strength;
            scroll=time*(pass==1 ? .03f : .003f)+(float)((ux*ux*3121+ux*45238971+uz*uz*418711+uz*13761)&31);
            glColor4f(1,1,1,alpha);
            glTexCoord2f(0,low*.25f+scroll); glVertex3f(x+.5f-sx,low,z+.5f-sz);
            glTexCoord2f(1,low*.25f+scroll); glVertex3f(x+.5f+sx,low,z+.5f+sz);
            glTexCoord2f(1,high*.25f+scroll); glVertex3f(x+.5f+sx,high,z+.5f+sz);
            glTexCoord2f(0,high*.25f+scroll); glVertex3f(x+.5f-sx,high,z+.5f-sz);
        }
        glEnd();
    }
    glMatrixMode(GL_TEXTURE); glPopMatrix(); glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(mode); glPopAttrib();
}
