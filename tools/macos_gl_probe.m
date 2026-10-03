/* Read-only context diagnostics for the self-hosted macOS runner.
 * Uses APIs available in the Snow Leopard SDK; no window or framebuffer effects. */
#import <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>
#include <stdio.h>

static void probe(const char *name,NSOpenGLPixelFormatAttribute *attributes)
{
    NSOpenGLPixelFormat *format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    NSOpenGLContext *context=nil;
    fprintf(stderr,"NSGL probe %s: format=%s",name,format?"yes":"no");
    if(format) {
        context=[[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
        fprintf(stderr," context=%s",context?"yes":"no");
    }
    fprintf(stderr,"\n");
    [context release]; [format release];
}

int main(void)
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    CGLRendererInfoObj info=NULL;
    GLint count=0,i;
    CGLError error=CGLQueryRendererInfo(~0u,&info,&count);
    NSOpenGLPixelFormatAttribute accelerated[]={NSOpenGLPFAAccelerated,NSOpenGLPFAClosestPolicy,
        NSOpenGLPFAColorSize,24,NSOpenGLPFAAlphaSize,8,NSOpenGLPFADepthSize,24,
        NSOpenGLPFAStencilSize,8,NSOpenGLPFADoubleBuffer,0};
    NSOpenGLPixelFormatAttribute relaxed[]={NSOpenGLPFAClosestPolicy,
        NSOpenGLPFAColorSize,24,NSOpenGLPFAAlphaSize,8,NSOpenGLPFADepthSize,24,
        NSOpenGLPFAStencilSize,8,NSOpenGLPFADoubleBuffer,0};
    NSOpenGLPixelFormatAttribute basic[]={NSOpenGLPFAColorSize,24,
        NSOpenGLPFADepthSize,16,NSOpenGLPFADoubleBuffer,0};
    NSOpenGLPixelFormatAttribute offline[]={NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFAColorSize,24,NSOpenGLPFADepthSize,16,NSOpenGLPFADoubleBuffer,0};
    NSOpenGLPixelFormatAttribute software[]={NSOpenGLPFARendererID,kCGLRendererGenericFloatID,
        NSOpenGLPFAColorSize,24,NSOpenGLPFADepthSize,16,NSOpenGLPFADoubleBuffer,0};
    NSOpenGLPixelFormatAttribute minimum[]={NSOpenGLPFARendererID,kCGLRendererGenericFloatID,0};
    fprintf(stderr,"CGL renderers: error=%s count=%d\n",CGLErrorString(error),(int)count);
    if(error==kCGLNoError) {
        for(i=0;i<count;++i) {
            GLint id=0,accelerated_value=0,online=0,display=0;
            CGLDescribeRenderer(info,i,kCGLRPRendererID,&id);
            CGLDescribeRenderer(info,i,kCGLRPAccelerated,&accelerated_value);
            CGLDescribeRenderer(info,i,kCGLRPOnline,&online);
            CGLDescribeRenderer(info,i,kCGLRPDisplayMask,&display);
            fprintf(stderr,"CGL renderer %d: id=0x%x accelerated=%d online=%d display=0x%x\n",
                (int)i,(unsigned)id,(int)accelerated_value,(int)online,(unsigned)display);
        }
        CGLDestroyRendererInfo(info);
    }
    probe("accelerated/default",accelerated); probe("relaxed/default",relaxed);
    probe("basic/depth16",basic); probe("offline/depth16",offline);
    probe("software/depth16",software); probe("software/minimum",minimum);
    [pool drain]; return 0;
}
