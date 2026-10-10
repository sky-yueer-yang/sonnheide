$input v_world,v_normal,v_uv,v_color,v_shadow
#include <bgfx_shader.sh>
#include "common.sh"
SAMPLER2D(s_shadow,3);
SAMPLER2D(s_waterDepth,4);
uniform vec4 u_waterDomain;
float pixelHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
float unpackDepth(vec4 c){return dot(c,vec4(1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0));}
void main(){
 vec2 worldXZ=v_world.xz+u_environment.zw;
 vec2 local=worldXZ-u_waterDomain.xy;float depthM=20.0;
 if(local.x>=0.0&&local.y>=0.0&&local.x<u_waterDomain.z&&local.y<u_waterDomain.w){
  vec2 uv=(floor(local/2.0)+0.5)*2.0/u_waterDomain.zw;depthM=texture2D(s_waterDepth,uv).r;
 }
 // Opaque legible blue pixels. Colour is based on actual frozen bed depth,
 // not navigation colours, shore smoothing or reflected sky.
 vec3 color=depthM<=3.0?vec3(0.12,0.47,0.79):depthM<=6.0?vec3(0.085,0.35,0.73):vec3(0.065,0.25,0.59);
 vec2 pixel=floor(worldXZ*8.0);vec2 cluster=floor(pixel/vec2(3.0,2.0));
 float mark=pixelHash(cluster);float motif=mark>0.87?1.10:mark<0.16?0.94:1.0;
 // Nearest pixel art at close range; unresolved subpixels become a solid
 // palette colour instead of a distracting distant moire pattern.
 float footprint=max(length(dFdx(worldXZ)),length(dFdy(worldXZ)));
 if(footprint>0.35)motif=1.0;
 color*=motif;
 vec3 shadow=v_shadow.xyz/v_shadow.w;shadow.xy=shadow.xy*vec2(0.5,-0.5)+0.5;
 float visibility=1.0;
 if(shadow.x>0.0&&shadow.x<1.0&&shadow.y>0.0&&shadow.y<1.0&&shadow.z>0.0&&shadow.z<1.0)
  visibility=step(shadow.z-0.0012,unpackDepth(texture2D(s_shadow,shadow.xy)));
 color*=mix(0.74,1.0,visibility)*mix(1.0,0.29,u_eyeAge.w);
 gl_FragColor=vec4(displayColor(color),1.0);
}
