$input v_world, v_normal, v_uv
#include <bgfx_shader.sh>
SAMPLER2D(s_base,0);
SAMPLER2D(s_normal,1);
SAMPLER2D(s_arm,2);
SAMPLER2D(s_sky,3);
SAMPLER2D(s_specular,4);
uniform vec4 u_eye;
uniform vec4 u_cameraForward;
uniform vec4 u_profile;
uniform vec4 u_sh[9];
#include "earth_common.sh"
// Triangle-grid candidates and transforms adapted from mmikk's MIT Hex-Tiling.
// Float phase avoids the upstream int multiplication overflow. Renderer only.
vec2 phase(vec2 p){return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);}
vec2 rotateUV(vec2 v,float a){float c=cos(a),s=sin(a);return vec2(c*v.x-s*v.y,s*v.x+c*v.y);}
void samplePBR(vec2 st,vec2 id,vec2 dx,vec2 dy,out vec3 color,out vec3 normal,out vec3 arm){
 vec2 center=vec2(id.x+0.5*id.y,id.y/1.15470054)/3.464101615;
 float a=(phase(id+vec2(23.0,71.0)).x-0.5)*1.5;
 vec2 uv=rotateUV(st-center,a)+center+phase(id);
 vec2 gx=rotateUV(dx,a),gy=rotateUV(dy,a);
 color=texture2DGrad(s_base,uv,gx,gy).rgb;
 vec3 n=texture2DGrad(s_normal,uv,gx,gy).rgb*2.0-1.0;
 // Inverse rotation for tangent components: texture rotation cannot rotate world light.
 vec2 slope=rotateUV(n.xy/max(n.z,0.05),-a);
 normal=vec3(slope,1.0);
 arm=texture2DGrad(s_arm,uv,gx,gy).rgb;
}
void main(){
 if(v_world.y<0.0)discard;
 vec2 st=v_uv+vec2(u_profile.w,u_profile.w*0.37);vec2 p=st*3.464101615;vec2 sk=vec2(p.x-0.57735027*p.y,1.15470054*p.y);vec2 b=floor(sk),f=fract(sk);
 float z=1.0-f.x-f.y,s=step(0.0,-z),ss=2.0*s-1.0;
 vec3 w=max(vec3(-z*ss,s-f.y*ss,s-f.x*ss),vec3_splat(0.0));
 // Same sharpened candidates/weights for every PBR channel, never three unrelated patterns.
 w=pow(w,vec3_splat(4.0));w/=max(dot(w,vec3_splat(1.0)),0.0001);
 vec3 c1,c2,c3,n1,n2,n3,a1,a2,a3;vec2 dx=dFdx(st),dy=dFdy(st);
 samplePBR(st,b+vec2(s,s),dx,dy,c1,n1,a1);
 samplePBR(st,b+vec2(s,1.0-s),dx,dy,c2,n2,a2);
 samplePBR(st,b+vec2(1.0-s,s),dx,dy,c3,n3,a3);
 vec3 albedo=c1*w.x+c2*w.y+c3*w.z;vec3 arm=a1*w.x+a2*w.y+a3*w.z;
 vec3 tangent=normalize(n1*w.x+n2*w.y+n3*w.z);
 vec3 gn=normalize(v_normal);vec3 tx=normalize(vec3(gn.y,-gn.x,0.0));vec3 tz=normalize(cross(tx,gn));
 vec3 n=normalize(tx*tangent.x+tz*tangent.y+gn*tangent.z);
 vec3 v=u_cameraForward.w>0.5?-normalize(u_cameraForward.xyz):normalize(u_eye.xyz-v_world),r=reflect(-v,n);float nv=max(dot(n,v),0.001);
 float rough=clamp(arm.y,0.06,1.0),metal=clamp(arm.z,0.0,1.0);
 vec3 f0=mix(vec3_splat(0.04),albedo,metal);vec3 F=f0+(1.0-f0)*pow(1.0-nv,5.0);
 vec3 diffuse=albedo*(1.0-F)*(1.0-metal)*diffuseSH(n)*arm.x;
 vec4 br=rough*vec4(-1.0,-0.0275,-0.572,0.022)+vec4(1.0,0.0425,1.04,-0.04);
 float a004=min(br.x*br.x,exp2(-9.28*nv))*br.x+br.y;
 vec2 AB=vec2(-1.04,1.04)*a004+br.zw;
 vec3 specular=environment(r,rough)*(f0*AB.x+AB.y)*arm.x;
 vec3 color=(diffuse+specular)*u_profile.x;
 float fog=1.0-exp(-length(u_eye.xyz-v_world)*u_profile.y);
 color=mix(color,environment(normalize(vec3(v_world.x-u_eye.x,0.03,v_world.z-u_eye.z)),0.6)*u_profile.x,fog*0.65);
 gl_FragColor=vec4(tone(color),1.0);
}
