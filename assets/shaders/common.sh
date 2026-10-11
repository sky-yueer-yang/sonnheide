uniform vec4 u_eyeAge;
uniform vec4 u_light;
uniform vec4 u_environment;
// Painted pixel colour, with stepped broad gradients. No PBR, reflections,
// celestial simulation or new simulation light/temperature state.
vec3 displayColor(vec3 radiance){return clamp(radiance,vec3_splat(0.0),vec3_splat(1.0));}
float artHash(vec2 p){return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453);}
vec3 skyColor(vec3 ray){
 float height=clamp(ray.y*1.6,0.0,1.0);height=floor(height*24.0)/24.0;
 vec3 day=mix(vec3(0.99,0.71,0.63),vec3(0.30,0.68,0.83),height);
 day=mix(day,vec3(0.26,0.43,0.71),max(0.0,height-0.5)*0.6);
 vec3 night=mix(vec3(0.22,0.18,0.39),vec3(0.045,0.075,0.19),height);
 return mix(day,night,u_eyeAge.w);
}
