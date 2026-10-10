$input v_uv
#include <bgfx_shader.sh>
#include "common.sh"
uniform vec4 u_cameraForward;
uniform vec4 u_cameraRight;
uniform vec4 u_cameraUp;
void main(){vec2 p=vec2(v_uv.x*2.0-1.0,1.0-v_uv.y*2.0);vec3 ray=normalize(u_cameraForward.xyz+p.x*u_cameraRight.xyz*u_cameraRight.w+p.y*u_cameraUp.xyz*u_cameraUp.w);gl_FragColor=vec4(displayColor(skyColor(ray)),1.0);}
