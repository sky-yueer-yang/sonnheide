$input v_world,v_normal,v_uv,v_color,v_shadow
#include <bgfx_shader.sh>
#include "common.sh"
SAMPLER2D(s_base,0);
SAMPLER2D(s_shadow,3);
float unpackDepth(vec4 c){return dot(c,vec4(1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0));}
void main(){
 float layer=floor(v_color.a*31.0+0.5);vec2 worldXZ=v_world.xz+u_environment.zw;
 vec2 faceUv=worldXZ/4.0;
 if(abs(v_normal.y)<0.7)faceUv=abs(v_normal.x)>0.7?vec2(worldXZ.y,-v_world.y)/4.0:vec2(worldXZ.x,-v_world.y)/4.0;
 vec2 tile=(floor(fract(faceUv)*128.0)+0.5)/128.0;
 vec3 albedo=texture2D(s_base,vec2(tile.x,(tile.y+layer)/32.0)).rgb*v_color.rgb;
 if(layer<24.0){
  // Large nonrepeating hue islands sit underneath fine atlas marks. Low
  // contrast keeps the image legible at all distances and on flat soil.
  float patch=artHash(floor(worldXZ/vec2(5.0,6.0)));
  albedo*=0.96+floor(patch*4.0)*0.025;
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
 gl_FragColor=vec4(displayColor(mix(daylight,night,u_eyeAge.w)),1.0);
}
