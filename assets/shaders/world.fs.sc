$input v_world,v_normal,v_uv,v_color,v_shadow
#include <bgfx_shader.sh>
#include "common.sh"
SAMPLER2D(s_base,0);
SAMPLER2D(s_shadow,3);
SAMPLER2D(s_mean,1);
#include "surface.sh"
float unpackDepth(vec4 c){return dot(c,vec4(1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0));}
// Continuous world-space colour only: the actual point-sampled atlas and
// authoritative coastline remain unchanged. Cubic interpolation makes tone
// and its first derivative continuous across the noise lattice boundaries.
float terrainToneNoise(vec2 p){
 vec2 lattice=floor(p),fraction=fract(p);vec2 blend=fraction*fraction*(3.0-2.0*fraction);
 float a=artHash(lattice),b=artHash(lattice+vec2(1.0,0.0)),c=artHash(lattice+vec2(0.0,1.0)),d=artHash(lattice+vec2(1.0,1.0));
 return mix(mix(a,b,blend.x),mix(c,d,blend.x),blend.y);
}
void main(){
 float layer=floor(v_color.a*31.0+0.5);vec2 worldXZ=v_world.xz+u_environment.zw;float coverage=1.0;
 if(u_surfaceMode.x>0.5){vec4 info=surfaceInfo(v_world.xz);float kind=floor(info.r*255.0+0.5);coverage=info.b;if(coverage<=0.0||kind>=5.0)discard;layer=kind==3.0?16.0:floor(info.g*255.0+0.5);}
 if(v_world.y<-.001)discard;
 vec2 faceUv=worldXZ/2.0;
 if(abs(v_normal.y)<0.7)faceUv=abs(v_normal.x)>0.7?vec2(worldXZ.y,-v_world.y)/2.0:vec2(worldXZ.x,-v_world.y)/2.0;
 vec2 tile=(floor(fract(faceUv)*128.0)+0.5)/128.0;
 vec3 albedo=texture2D(s_base,vec2(tile.x,(tile.y+layer)/32.0)).rgb*v_color.rgb;
 float footprint=max(length(dFdx(worldXZ)),length(dFdy(worldXZ)));
 float distant=clamp((footprint-0.020)/0.080,0.0,1.0);
 if(layer<24.0)albedo=mix(albedo,texture2D(s_mean,vec2(0.5,(layer+0.5)/32.0)).rgb*v_color.rgb,distant);
 if(layer<24.0){
  // Rotated, incommensurate continuous fields produce gentle irregular
  // variation rather than visible5x6m rectangles or management-cell seams.
  vec2 tonePosition=vec2(worldXZ.x*0.8+worldXZ.y*0.6,-worldXZ.x*0.6+worldXZ.y*0.8)/10.0;
  float tone=(terrainToneNoise(tonePosition)+0.4*terrainToneNoise(tonePosition*2.17+vec2(41.7,19.3)))/1.4;
  albedo*=0.97+tone*0.06;
 }
 vec3 shadow=v_shadow.xyz/v_shadow.w;shadow.xy=shadow.xy*vec2(0.5,-0.5)+0.5;
 float visibility=1.0;
 if(shadow.x>0.0&&shadow.x<1.0&&shadow.y>0.0&&shadow.y<1.0&&shadow.z>0.0&&shadow.z<1.0){
  float bias=max(0.0007,0.0017*(1.0-dot(v_normal,normalize(-u_light.xyz))));
  visibility=step(shadow.z-bias,unpackDepth(texture2D(s_shadow,shadow.xy)));
 }
 float directional=max(dot(normalize(v_normal),normalize(-u_light.xyz)),0.0);
 vec3 daylight=albedo*(0.65+0.35*directional)*mix(vec3(0.70,0.78,0.94),vec3(1.03,1.01,0.96),visibility);
 vec3 night=albedo*(0.35+0.18*directional)*mix(vec3(0.64,0.69,0.94),vec3(0.86,0.95,1.09),visibility);
 gl_FragColor=vec4(displayColor(mix(daylight,night,u_eyeAge.w)),coverage);
}
