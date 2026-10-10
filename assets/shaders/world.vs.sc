$input a_position, a_normal, a_texcoord0, a_color0
$output v_world, v_normal, v_uv, v_color, v_shadow
#include <bgfx_shader.sh>
uniform mat4 u_lightMatrix;
void main(){v_world=mul(u_model[0],vec4(a_position,1.0)).xyz;v_normal=a_normal;v_uv=a_texcoord0;v_color=a_color0;v_shadow=mul(u_lightMatrix,vec4(v_world,1.0));gl_Position=mul(u_modelViewProj,vec4(a_position,1.0));}
