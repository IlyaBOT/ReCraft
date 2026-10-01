#include "menu_background.h"
#include <string.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

void menu_background_clear(MenuBackground *bg)
{
    if (bg->texture) glDeleteTextures(1,&bg->texture);
    memset(bg,0,sizeof(*bg));
}

int menu_background_prepare(MenuBackground *bg,int width,int height,int blur,
                            int live,int *scene_width,int *scene_height)
{
    int w=width,h=height,tw=1,th=1;
    GLint maximum,binding;
    if (live && !blur) {
        menu_background_clear(bg);
        *scene_width=width; *scene_height=height;
        return 1;
    }
    if (blur && (w>512 || h>512)) {
        float scale=512.0f/(w>h?w:h);
        w=(int)(w*scale); h=(int)(h*scale);
    }
    if (w<1) w=1;
    if (h<1) h=1;
    *scene_width=w; *scene_height=h;
    if (bg->screen_width==width && bg->screen_height==height && bg->blur==blur) {
        if (!bg->texture) { *scene_width=width; *scene_height=height; }
        return live || !bg->valid;
    }
    menu_background_clear(bg);
    bg->width=w; bg->height=h;
    bg->screen_width=width; bg->screen_height=height; bg->blur=blur;
    while (tw<w) tw*=2;
    while (th<h) th*=2;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    /* At very large resolutions leave the frozen world rendered normally,
     * rather than allocate an oversized cache on an integrated legacy GPU. */
    if (tw>maximum || th>maximum || (unsigned)tw*(unsigned)th>2097152u) {
        *scene_width=width; *scene_height=height;
        return 1;
    }
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    glGenTextures(1,&bg->texture);
    glBindTexture(GL_TEXTURE_2D,bg->texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,blur?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,blur?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,tw,th,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);
    if (glGetError()!=GL_NO_ERROR) {
        glDeleteTextures(1,&bg->texture); bg->texture=0;
        *scene_width=width; *scene_height=height;
    }
    glBindTexture(GL_TEXTURE_2D,(GLuint)binding);
    bg->texture_width=tw; bg->texture_height=th;
    return 1;
}

void menu_background_capture(MenuBackground *bg)
{
    GLint binding;
    if (!bg->texture) return;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    glBindTexture(GL_TEXTURE_2D,bg->texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,bg->width,bg->height);
    bg->valid=glGetError()==GL_NO_ERROR;
    glBindTexture(GL_TEXTURE_2D,(GLuint)binding);
}

void menu_background_draw(const MenuBackground *bg)
{
    float u0,v0,u1,v1;
    int i,passes=bg->blur?4:1;
    if (!bg->valid) return;
    u0=(bg->blur?1.0f:0.5f)/bg->texture_width;
    v0=(bg->blur?1.0f:0.5f)/bg->texture_height;
    u1=(bg->width-(bg->blur?1.0f:0.5f))/bg->texture_width;
    v1=(bg->height-(bg->blur?1.0f:0.5f))/bg->texture_height;
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST); glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,bg->texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for (i=0;i<passes;++i) {
        float du=bg->blur?(i&1?0.5f:-0.5f)/bg->texture_width:0;
        float dv=bg->blur?(i&2?0.5f:-0.5f)/bg->texture_height:0;
        glColor4f(1,1,1,1.0f/(i+1));
        glBegin(GL_QUADS);
        glTexCoord2f(u0+du,v1+dv); glVertex2i(0,0);
        glTexCoord2f(u1+du,v1+dv); glVertex2i(bg->screen_width,0);
        glTexCoord2f(u1+du,v0+dv); glVertex2i(bg->screen_width,bg->screen_height);
        glTexCoord2f(u0+du,v0+dv); glVertex2i(0,bg->screen_height);
        glEnd();
    }
    glPopAttrib();
}
