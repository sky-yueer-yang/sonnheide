$input v_world,v_normal,v_uv,v_color,v_shadow
#include <bgfx_shader.sh>
#include "common.sh"
SAMPLER2D(s_shadow,3);
SAMPLER2D(s_waterDepth,4);
uniform vec4 u_waterDomain;
float unpackDepth(vec4 c){return dot(c,vec4(1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0));}
void main(){
 vec2 worldXZ=v_world.xz+u_environment.zw;
 vec2 local=worldXZ-u_waterDomain.xy;float depthM=20.0;
 if(local.x>=0.0&&local.y>=0.0&&local.x<u_waterDomain.z&&local.y<u_waterDomain.w){
  float cell=u_environment.x;
  vec2 uv=(floor(local/cell)+0.5)*cell/u_waterDomain.zw;depthM=texture2D(s_waterDepth,uv).r;
 }
 // Actual frozen bed determines depth. The animated colour marks below never
 // move the water plane, create foam inventory or alter navigable geometry.
 vec3 color=depthM<=3.0?vec3(0.23,0.73,0.75):depthM<=10.0?vec3(0.12,0.55,0.70):vec3(0.10,0.38,0.62);
 vec2 broad=floor(worldXZ*0.55);float island=artHash(broad);
 color*=0.96+floor(island*3.0)*0.018;
 float time=u_environment.y;
 vec2 pixel=floor(worldXZ*32.0)/32.0;
 float flow=sin(pixel.x*0.75+pixel.y*1.31+time*0.72)+sin(pixel.y*2.3-pixel.x*0.17-time*0.46);
 float wave=step(1.37,flow)*step(0.57,artHash(floor(pixel*vec2(3.0,9.0))));
 float footprint=max(length(dFdx(worldXZ)),length(dFdy(worldXZ)));
 float fine=1.0-clamp((footprint-0.04)/0.22,0.0,1.0);
 color=mix(color,color+vec3(0.13,0.16,0.13),wave*fine*0.62);
 vec3 shadow=v_shadow.xyz/v_shadow.w;shadow.xy=shadow.xy*vec2(0.5,-0.5)+0.5;float visibility=1.0;
 if(shadow.x>0.0&&shadow.x<1.0&&shadow.y>0.0&&shadow.y<1.0&&shadow.z>0.0&&shadow.z<1.0)
  visibility=step(shadow.z-0.0012,unpackDepth(texture2D(s_shadow,shadow.xy)));
 color*=mix(vec3(0.78,0.84,1.0),vec3(1.0,1.0,1.0),visibility);
 color=mix(color,color*vec3(0.31,0.40,0.73),u_eyeAge.w);
 gl_FragColor=vec4(displayColor(color),1.0);
}
