$input v_uv,v_color
#include <bgfx_shader.sh>
SAMPLER2D(s_ui,0);
void main(){gl_FragColor=texture2D(s_ui,v_uv)*v_color;}
