// Equirectangular radiance, linear units; fixed Age profiles, no astronomy.
vec2 skyUV(vec3 d){return vec2(atan2(d.z,d.x)*0.159154943+0.5,acos(clamp(d.y,-1.0,1.0))*0.318309886);}
vec3 tone(vec3 c){c=max(c,vec3(0.0));c=(c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14);return pow(clamp(c,vec3(0.0),vec3(1.0)),vec3(1.0/2.2));}
vec3 environment(vec3 d,float roughness){return roughness<=0.0?texture2DLod(s_sky,skyUV(d),0.0).rgb:texture2DLod(s_specular,skyUV(d),roughness*9.0).rgb;}
vec3 diffuseSH(vec3 n){return max(vec3(0.0),u_sh[0].rgb+u_sh[1].rgb*n.y+u_sh[2].rgb*n.z+u_sh[3].rgb*n.x+u_sh[4].rgb*n.x*n.y+u_sh[5].rgb*n.y*n.z+u_sh[6].rgb*(3.0*n.z*n.z-1.0)+u_sh[7].rgb*n.x*n.z+u_sh[8].rgb*(n.x*n.x-n.y*n.y));}
