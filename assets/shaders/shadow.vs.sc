$input a_position, a_texcoord0, a_color0
$output v_shadow
#include <bgfx_shader.sh>
uniform vec4 u_environment;
void main(){
 vec3 position=a_position;float layer=floor(a_color0.a*31.0+0.5);
 if(layer>=25.0&&layer<27.0){
  position.x+=sin(u_environment.y*1.15+a_texcoord0.x+position.x*0.24+position.z*0.17)*0.035*a_texcoord0.y;
  position.z+=sin(u_environment.y*0.83+a_texcoord0.x)*0.018*a_texcoord0.y;
 }
 v_shadow=mul(u_modelViewProj,vec4(position,1.0));gl_Position=v_shadow;
}
