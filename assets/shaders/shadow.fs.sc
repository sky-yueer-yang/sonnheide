$input v_shadow
#include <bgfx_shader.sh>
vec4 packDepth(float d){vec4 p=fract(min(d,0.999999)*vec4(1.0,255.0,65025.0,16581375.0));p-=p.yzww*vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);return p;}
void main(){gl_FragColor=packDepth(v_shadow.z/v_shadow.w);}
