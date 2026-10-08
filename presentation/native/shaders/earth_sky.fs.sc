$input v_uv
#include <bgfx_shader.sh>
SAMPLER2D(s_sky,3);
SAMPLER2D(s_specular,4);
uniform vec4 u_profile;
uniform vec4 u_eye;
uniform vec4 u_cameraRight;
uniform vec4 u_cameraUp;
uniform vec4 u_cameraForward;
uniform vec4 u_sh[9];
#include "earth_common.sh"
void main(){vec2 p=vec2(v_uv.x*2.0-1.0,1.0-v_uv.y*2.0);
 vec3 ray=u_cameraForward.w>0.5?normalize(u_cameraForward.xyz):normalize(u_cameraForward.xyz+p.x*u_cameraRight.xyz*u_cameraRight.w+p.y*u_cameraUp.xyz*u_cameraUp.w);
 vec3 color=environment(ray,0.0)*u_profile.x;
 if(ray.y<0.0){vec3 N=vec3(0.0,1.0,0.0);float fresnel=0.02+0.98*pow(1.0+ray.y,5.0);
  vec3 reflectance=environment(reflect(ray,N),0.12)*u_profile.x;
  vec3 water=vec3(0.018,0.105,0.125)*u_profile.z;
  color=mix(water,reflectance,fresnel);
 }
 gl_FragColor=vec4(tone(color),1.0);
}
