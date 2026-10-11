$input a_position, a_normal, a_texcoord0, a_color0
$output v_world, v_normal, v_uv, v_color, v_shadow
#include <bgfx_shader.sh>
uniform mat4 u_lightMatrix;
uniform vec4 u_environment;
void main(){
 vec3 position=a_position;float layer=floor(a_color0.a*31.0+0.5);
 // Purely decorative grass and flowers bend. Core terrain vertices never do.
 if(layer>=25.0&&layer<27.0){
  float wind=sin(u_environment.y*1.15+a_texcoord0.x+position.x*0.24+position.z*0.17);
  position.x+=wind*0.035*a_texcoord0.y;position.z+=sin(u_environment.y*0.83+a_texcoord0.x)*0.018*a_texcoord0.y;
 }
 v_world=mul(u_model[0],vec4(position,1.0)).xyz;v_normal=a_normal;v_uv=a_texcoord0;v_color=a_color0;
 v_shadow=mul(u_lightMatrix,vec4(v_world,1.0));gl_Position=mul(u_modelViewProj,vec4(position,1.0));
}
