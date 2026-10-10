$input v_world,v_normal,v_uv,v_color,v_shadow
#include <bgfx_shader.sh>
#include "common.sh"
SAMPLER2D(s_base,0);
SAMPLER2D(s_shadow,3);
float unpackDepth(vec4 c){return dot(c,vec4(1.0,1.0/255.0,1.0/65025.0,1.0/16581375.0));}
void main(){
 vec2 faceUv=v_uv;
 if(abs(v_normal.y)<0.7){
  faceUv=abs(v_normal.x)>0.7?vec2(v_world.z+u_environment.w,-v_world.y)/2.0:vec2(v_world.x+u_environment.z,-v_world.y)/2.0;
 }
 // Explicit pixel centre: 16x16 tiles, 24 top/side material layers.
 vec2 tile=(floor(fract(faceUv)*16.0)+0.5)/16.0;
 vec2 uv=vec2(tile.x,(tile.y+floor(v_color.a*23.0+0.5))/24.0);
 vec3 albedo=texture2D(s_base,uv).rgb*v_color.rgb;
 vec3 shadow=v_shadow.xyz/v_shadow.w;shadow.xy=shadow.xy*vec2(0.5,-0.5)+0.5;
 float visibility=1.0;
 if(shadow.x>0.0&&shadow.x<1.0&&shadow.y>0.0&&shadow.y<1.0&&shadow.z>0.0&&shadow.z<1.0){
  float bias=max(0.0007,0.0017*(1.0-dot(v_normal,normalize(-u_light.xyz))));
  visibility=step(shadow.z-bias,unpackDepth(texture2D(s_shadow,shadow.xy)));
 }
 float directional=max(dot(normalize(v_normal),normalize(-u_light.xyz)),0.0);
 float brightness=mix(0.46+0.54*directional*mix(0.58,1.0,visibility),0.24+0.18*directional*visibility,u_eyeAge.w);
 gl_FragColor=vec4(displayColor(albedo*brightness),1.0);
}
