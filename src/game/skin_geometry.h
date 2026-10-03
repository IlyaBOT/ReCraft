#ifndef RECRAFT_SKIN_GEOMETRY_H
#define RECRAFT_SKIN_GEOMETRY_H
/* ModelRenderer/TexturedQuad's legacy layout, including geometric mirroring.
 * Keep this calculation independent of OpenGL so atlas orientation is testable. */
static inline void skin_vertex(float x,float y,float z,int w,int h,int d,int u,int v,
                               int mirror,int atlas_height,float inflate,int f,int i,
                               float position[3],float texture[2])
{
    static const int face[6][4]={{5,1,2,6},{0,4,7,3},{5,4,0,1},{2,3,7,6},{1,0,3,2},{4,5,6,7}};
    const int uv[6][4]={{u+d+w,v+d,u+d+w+d,v+d+h},{u,v+d,u+d,v+d+h},
        {u+d,v,u+d+w,v+d},{u+d+w,v,u+d+w+w,v+d},{u+d,v+d,u+d+w,v+d+h},
        {u+d+w+d,v+d,u+d+w+d+w,v+d+h}};
    static const unsigned char high[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    int c=mirror?3-i:i, index=face[f][c];
    position[0]=x-inflate+((int)high[index][0]^!!mirror)*(w+inflate*2);
    position[1]=y-inflate+high[index][1]*(h+inflate*2);
    position[2]=z-inflate+high[index][2]*(d+inflate*2);
    texture[0]=(uv[f][(c==0 || c==3)?2:0]+((c==0 || c==3)?-.1f:.1f))/64;
    texture[1]=(uv[f][c<2?1:3]+(c<2?.1f:-.1f))/atlas_height;
}
#endif
