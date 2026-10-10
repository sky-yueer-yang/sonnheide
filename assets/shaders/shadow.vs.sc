$input a_position
$output v_shadow
#include <bgfx_shader.sh>
void main(){v_shadow=mul(u_modelViewProj,vec4(a_position,1.0));gl_Position=v_shadow;}
