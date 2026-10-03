#include "texture_animation.h"
#include <math.h>
#include <string.h>
static float random_float(TextureAnimation *fx)
{ fx->random=fx->random*1664525u+1013904223u; return (float)(fx->random>>8)/16777216; }
void texture_animation_init(TextureAnimation *fx)
{
    int frame,x,y,layer;
    uint64_t seed=(100^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    memset(fx,0,sizeof(*fx)); fx->random=100;
    for(frame=0;frame<32;++frame) for(x=0;x<16;++x) for(y=0;y<16;++y) {
        float value=0; uint8_t *p=fx->portal[frame]+(y*16+x)*4;
        for(layer=0;layer<2;++layer) {
            float a=(x-layer*8)/8.0f,b=(y-layer*8)/8.0f,r,angle;
            if(a< -1) a+=2;
            if(a>=1) a-=2;
            if(b< -1) b+=2;
            if(b>=1) b-=2;
            r=a*a+b*b;
            angle=atan2f(b,a)+(frame/32.0f*6.2831853f-r*10+layer*2)*(layer*2-1);
            value+=(sinf(angle)+1)*.25f/(r+1);
        }
        seed=(seed*UINT64_C(0x5deece66d)+11)&UINT64_C(0xffffffffffff);
        value+=(float)(seed>>24)/16777216*.1f;
        p[0]=(uint8_t)(value*value*200+55); p[1]=(uint8_t)(value*value*value*value*255);
        p[2]=p[3]=(uint8_t)(value*100+155);
    }
    texture_animation_step(fx);
}
void texture_animation_step(TextureAnimation *fx)
{
    int x,y,i,a,b;
    for(x=0;x<16;++x) for(y=0;y<20;++y) {
        float value=fx->fire[0][x+((y+1)%20)*16]*18;int samples=18;
        for(a=x-1;a<=x+1;++a) for(b=y;b<=y+1;++b) {
            if(a>=0 && a<16 && b<20) value+=fx->fire[0][a+b*16];
            ++samples;
        }
        fx->fire[1][x+y*16]=value/(samples*1.06f);
        if(y==19) fx->fire[1][x+y*16]=random_float(fx)*random_float(fx)*random_float(fx)*4+random_float(fx)*.1f+.2f;
    }
    memcpy(fx->fire[0],fx->fire[1],sizeof(fx->fire[0]));
    for(i=0;i<256;++i) {
        float value=fmaxf(0,fminf(1,fx->fire[0][i]*1.8f)),square=value*value;
        uint8_t *p=fx->fire_pixels+i*4;
        p[0]=(uint8_t)(value*155+100);p[1]=(uint8_t)(square*255);p[2]=(uint8_t)(square*square*square*square*square*255);p[3]=value<.5f?0:255;
    }
    for(x=0;x<16;++x) for(y=0;y<16;++y) {
        float water=0,lava=0,soup=0;
        int ox=(int)(sinf(y*6.2831853f/16)*1.2f),oy=(int)(sinf(x*6.2831853f/16)*1.2f);
        i=x+y*16;
        for(a=-1;a<=1;++a) {
            water+=fx->water[0][((x+a)&15)+y*16];
            for(b=-1;b<=1;++b) lava+=fx->lava[0][((x+a+ox)&15)+((y+b+oy)&15)*16];
        }
        for(a=0;a<2;++a) for(b=0;b<2;++b) soup+=fx->lava[2][((x+a)&15)+((y+b)&15)*16];
        fx->water[1][i]=water/3.3f+fx->water[2][i]*.8f;
        fx->lava[1][i]=lava/10+soup*.2f;
        fx->lava[2][i]+=fx->lava[3][i]*.01f;
        if(fx->lava[2][i]<0) fx->lava[2][i]=0;
        fx->lava[3][i]-=.06f; if(random_float(fx)<.005f) fx->lava[3][i]=1.5f;
    }
    for(i=0;i<256;++i) {
        float water,lava; uint8_t *w=fx->water_pixels+i*4,*l=fx->lava_pixels+i*4;
        fx->water[2][i]+=fx->water[3][i]*.05f;
        if(fx->water[2][i]<0) fx->water[2][i]=0;
        fx->water[3][i]-=.1f; if(random_float(fx)<.05f) fx->water[3][i]=.5f;
        fx->water[0][i]=fx->water[1][i]; fx->lava[0][i]=fx->lava[1][i];
        water=fx->water[0][i]; lava=fx->lava[0][i]*2;
        if(water<0) water=0;
        if(water>1) water=1;
        water*=water;
        if(lava<0) lava=0;
        if(lava>1) lava=1;
        w[0]=(uint8_t)(32+water*32); w[1]=(uint8_t)(50+water*64); w[2]=255; w[3]=(uint8_t)(146+water*50);
        l[0]=(uint8_t)(155+lava*100); l[1]=(uint8_t)(lava*lava*255); l[2]=(uint8_t)(lava*lava*lava*lava*128); l[3]=255;
    }
}
