#ifndef RECRAFT_MACOS_TEST_CONTEXT_H
#define RECRAFT_MACOS_TEST_CONTEXT_H
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/glext.h>

/* Native legacy OpenGL drawable without NSWindow/WindowServer. All rendering
 * assertions use this real framebuffer, including glReadPixels and GUI assets. */
static CGLContextObj test_context;
static GLuint test_framebuffer,test_color,test_depth;
static int test_cgl_context(int width,int height)
{
    CGLPixelFormatAttribute attributes[]={kCGLPFARendererID,kCGLRendererGenericFloatID,kCGLPFAAllowOfflineRenderers,
        kCGLPFAColorSize,24,kCGLPFAAlphaSize,8,kCGLPFADepthSize,24,
        kCGLPFAStencilSize,8,0};
    CGLPixelFormatObj format=NULL;
    GLint count=0;
    CGLError error;
    const char *stage="choose pixel format";
    int pw=1,ph=1;
    error=CGLChoosePixelFormat(attributes,&format,&count);
    if(error!=kCGLNoError || !format) goto failed;
    stage="create software context";
    error=CGLCreateContext(format,NULL,&test_context);
    CGLDestroyPixelFormat(format); format=NULL;
    if(error!=kCGLNoError) goto failed;
    stage="make context current";
    error=CGLSetCurrentContext(test_context);
    if(error!=kCGLNoError) goto failed;
    while(pw<width) pw*=2;
    while(ph<height) ph*=2;
    stage="allocate offscreen test framebuffer";
    glGenTextures(1,&test_color); glBindTexture(GL_TEXTURE_2D,test_color);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,pw,ph,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenRenderbuffersEXT(1,&test_depth); glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,test_depth);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT24,pw,ph);
    glGenFramebuffersEXT(1,&test_framebuffer); glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,test_framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,test_color,0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,test_depth);
    if(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT || glGetError()!=GL_NO_ERROR) {
        error=kCGLBadDrawable; goto failed;
    }
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glBindTexture(GL_TEXTURE_2D,0);
    glViewport(0,0,width,height);
    fprintf(stderr,"renderer_test: native CGL offscreen test framebuffer %dx%d ready\n",pw,ph);
    return 1;
failed:
    fprintf(stderr,"renderer_test: CGL %s failed: %s\n",stage,CGLErrorString(error));
    if(format) CGLDestroyPixelFormat(format);
    if(test_framebuffer) glDeleteFramebuffersEXT(1,&test_framebuffer);
    if(test_depth) glDeleteRenderbuffersEXT(1,&test_depth);
    if(test_color) glDeleteTextures(1,&test_color);
    CGLSetCurrentContext(NULL);
    if(test_context) CGLDestroyContext(test_context);
    test_context=NULL; test_framebuffer=test_color=test_depth=0;
    return 0;
}
static void test_cgl_close(void)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glDeleteFramebuffersEXT(1,&test_framebuffer);
    glDeleteRenderbuffersEXT(1,&test_depth); glDeleteTextures(1,&test_color);
    CGLSetCurrentContext(NULL); CGLDestroyContext(test_context);
}
#endif
