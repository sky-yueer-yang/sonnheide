uniform vec4 u_eyeAge;
uniform vec4 u_light;
uniform vec4 u_environment;
// Categorical pixel colour. No filmic tonemapping, haze, celestial simulation,
// smooth noise clouds, specular highlights or physically based material model.
vec3 displayColor(vec3 radiance){return clamp(radiance,vec3_splat(0.0),vec3_splat(1.0));}
vec3 skyColor(vec3 ray){
 vec3 day=ray.y<0.08?vec3(0.30,0.57,0.91):vec3(0.23,0.49,0.88);
 return mix(day,vec3(0.012,0.024,0.068),u_eyeAge.w);
}
