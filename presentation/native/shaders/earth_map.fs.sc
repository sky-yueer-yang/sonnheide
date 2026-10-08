$input v_uv
#include <bgfx_shader.sh>
SAMPLER2D(s_base,0);
void main(){gl_FragColor=texture2D(s_base,v_uv);}
