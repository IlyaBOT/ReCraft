#include "climate.h"
#include <limits.h>
static unsigned bits(uint64_t *state,unsigned count)
{ *state=(*state*UINT64_C(0x5deece66d)+11)&UINT64_C(0xffffffffffff); return (unsigned)(*state>>(48-count)); }
static unsigned bounded(uint64_t *state,unsigned bound)
{
    unsigned b,n;
    do { b=bits(state,31); if(!(bound&(bound-1))) return (unsigned)(((uint64_t)b*bound)>>31); n=b%bound; }
    while((uint64_t)b-n+bound-1>INT32_MAX);
    return n;
}
static double random_double(uint64_t *state)
{ uint64_t high=bits(state,26); return (double)((high<<27)+bits(state,27))/9007199254740992.0; }
static void noise_init(BetaNoise2 *noise,uint64_t *state)
{
    unsigned i;
    noise->x=random_double(state)*256; noise->z=random_double(state)*256;
    (void)random_double(state); /* The unused third offset still consumes RNG. */
    for(i=0;i<256;++i) noise->permutation[i]=(uint16_t)i;
    for(i=0;i<256;++i) {
        unsigned j=i+bounded(state,256-i); uint16_t t=noise->permutation[i];
        noise->permutation[i]=noise->permutation[j]; noise->permutation[j]=t;
        noise->permutation[i+256]=noise->permutation[i];
    }
}
void beta_climate_init(BetaClimate *c,uint64_t seed)
{
    unsigned i; uint64_t state;
    state=(seed*9871^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    for(i=0;i<4;++i) noise_init(&c->temperature[i],&state);
    state=(seed*39811^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    for(i=0;i<4;++i) noise_init(&c->humidity[i],&state);
    state=(seed*543321^UINT64_C(0x5deece66d))&UINT64_C(0xffffffffffff);
    for(i=0;i<2;++i) noise_init(&c->detail[i],&state);
}
static double corner(unsigned index,double x,double z)
{
    static const int gradient[12][2]={{1,1},{-1,1},{1,-1},{-1,-1},{1,0},{-1,0},{1,0},{-1,0},{0,1},{0,-1},{0,1},{0,-1}};
    double t=.5-x*x-z*z; const int *g=gradient[index%12];
    if(t<0) return 0;
    t*=t; return t*t*(g[0]*x+g[1]*z);
}
static double noise_sample(const BetaNoise2 *n,double x,double z)
{
    const double f=.3660254037844386,g=.21132486540518713;
    double skew=(x+z)*f,ax=x+skew,az=z+skew,unskew,x0,z0,x1,z1,x2,z2;
    int ix=ax>0 ? (int)ax : (int)ax-1,iz=az>0 ? (int)az : (int)az-1;
    int a,b; unsigned u=(unsigned)ix&255,v=(unsigned)iz&255;
    unskew=(ix+iz)*g; x0=x-(ix-unskew); z0=z-(iz-unskew);
    a=x0>z0; b=!a; x1=x0-a+g; z1=z0-b+g; x2=x0-1+2*g; z2=z0-1+2*g;
    return 70*(corner(n->permutation[u+n->permutation[v]],x0,z0)+
        corner(n->permutation[u+a+n->permutation[v+b]],x1,z1)+
        corner(n->permutation[u+1+n->permutation[v+1]],x2,z2));
}
static double octaves(const BetaNoise2 *noise,unsigned count,int x,int z,double scale,double decay)
{
    unsigned i; double sum=0,frequency=1,amplitude=1;
    scale/=1.5;
    for(i=0;i<count;++i) {
        double rate=scale*frequency;
        sum+=noise_sample(noise+i,x*rate+noise[i].x,z*rate+noise[i].z)*(.55/amplitude);
        frequency*=decay; amplitude*=.5;
    }
    return sum;
}
static double clamp(double v) { return v<0 ? 0 : v>1 ? 1 : v; }
void beta_climate_sample(const BetaClimate *c,int x,int z,double *t,double *h)
{
    double detail=octaves(c->detail,2,x,z,.25,.5882352941176471)*1.1+.5;
    double temperature=(octaves(c->temperature,4,x,z,.02500000037252903,.25)*.15+.7)*.99+detail*.01;
    double humidity=(octaves(c->humidity,4,x,z,.05000000074505806,.3333333333333333)*.15+.5)*.998+detail*.002;
    *t=clamp(1-(1-temperature)*(1-temperature)); *h=clamp(humidity);
}
unsigned beta_climate_precipitation(double t,double h)
{
    /* getBiomeFromLookup uses a 64 x 64 table filled using float arithmetic. */
    float temperature=(float)(int)(t*63)/63,humidity=(float)(int)(h*63)/63;
    humidity*=temperature;
    if(temperature<.1f || (humidity<.2f && temperature<.5f)) return 2; /* Tundra. */
    if(humidity<.2f) return temperature<.95f ? 1 : 0; /* Savanna/desert. */
    if(humidity>.5f && temperature<.7f) return 1; /* Swamp. */
    return temperature<.5f ? 2 : 1; /* Taiga and other rain biomes. */
}
