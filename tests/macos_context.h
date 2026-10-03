#ifndef RECRAFT_MACOS_TEST_CONTEXT_H
#define RECRAFT_MACOS_TEST_CONTEXT_H
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>

/* Native legacy OpenGL drawable without NSWindow/WindowServer. All rendering
 * assertions use this real framebuffer, including glReadPixels and GUI assets. */
static CGLContextObj test_context;
static CGLPBufferObj test_pbuffer;
static int test_cgl_context(int width,int height)
{
    CGLPixelFormatAttribute attributes[]={kCGLPFARendererID,kCGLRendererGenericFloatID,kCGLPFAAllowOfflineRenderers,
        kCGLPFAPBuffer,kCGLPFAColorSize,24,kCGLPFAAlphaSize,8,kCGLPFADepthSize,24,
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
    stage="create pbuffer";
    error=CGLCreatePBuffer(pw,ph,GL_TEXTURE_2D,GL_RGBA,0,&test_pbuffer);
    if(error!=kCGLNoError) goto failed;
    stage="attach pbuffer";
    error=CGLSetPBuffer(test_context,test_pbuffer,0,0,0);
    if(error!=kCGLNoError) goto failed;
    glViewport(0,0,width,height);
    fprintf(stderr,"renderer_test: native CGL pbuffer %dx%d ready\n",pw,ph);
    return 1;
failed:
    fprintf(stderr,"renderer_test: CGL %s failed: %s\n",stage,CGLErrorString(error));
    if(format) CGLDestroyPixelFormat(format);
    CGLSetCurrentContext(NULL);
    if(test_context) CGLDestroyContext(test_context);
    if(test_pbuffer) CGLDestroyPBuffer(test_pbuffer);
    test_context=NULL; test_pbuffer=NULL;
    return 0;
}
static void test_cgl_close(void)
{
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(test_context); CGLDestroyPBuffer(test_pbuffer);
}
#endif
