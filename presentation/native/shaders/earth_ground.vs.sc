$input a_position, a_normal, a_texcoord0
$output v_world, v_normal, v_uv
#include <bgfx_shader.sh>
void main(){v_world=a_position;v_normal=a_normal;v_uv=a_texcoord0;gl_Position=mul(u_modelViewProj,vec4(a_position,1.0));}
