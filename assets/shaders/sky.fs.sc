$input v_uv
#include <bgfx_shader.sh>
#include "common.sh"
uniform vec4 u_menuScene;
uniform vec4 u_cameraForward;
uniform vec4 u_cameraRight;
uniform vec4 u_cameraUp;
float ellipse(vec2 p,vec2 centre,vec2 radii){vec2 d=(p-centre)/radii;return 1.0-step(1.0,dot(d,d));}
vec3 illustratedSky(vec3 ray){
 vec3 color=skyColor(ray);vec3 pixelRay=normalize(floor(ray*240.0)/240.0);
 vec3 sunDirection=normalize(vec3(0.22,0.20,0.98));float solar=dot(pixelRay,sunDirection);
 float halo=step(0.9948,solar);float disk=step(0.9987,solar);
 color=mix(color,mix(vec3(1.0,0.6666667,0.0),vec3(0.45,0.45,0.76),u_eyeAge.w),halo*0.28);
 color=mix(color,mix(vec3(1.0,0.6666667,0.0),vec3(0.88,0.91,1.0),u_eyeAge.w),disk);
 if(ray.y>0.01){
  // Fine stair-step cloud silhouettes drift in two independent painted layers.
  // They are backdrop art, not huge solid cuboids floating near the camera.
  vec2 skyPlane=ray.xz/(ray.y+0.24)*2.1;
  for(int layer=0;layer<2;++layer){
   float scale=layer==0?1.0:1.63;
   vec2 q=skyPlane*scale+vec2(u_environment.y*(layer==0?0.007:0.011),float(layer)*17.0);
   q=floor(q*120.0)/120.0;vec2 cell=floor(q);vec2 p=fract(q)-0.5;
   float seed=artHash(cell+float(layer)*13.0);p+=vec2(seed-0.5,artHash(cell+4.0)-0.5)*0.19;
   float body=max(ellipse(p,vec2(-0.18,-0.005),vec2(0.20,0.090)),ellipse(p,vec2(0.10,0.012),vec2(0.27,0.100)));
   body=max(body,ellipse(p,vec2(0.0,-0.070),vec2(0.15,0.095)));
   body*=step(0.24,seed);float shadowBand=step(0.043,p.y);
   vec3 cloud=mix(vec3(1.0,0.89,0.81),vec3(0.81,0.80,0.88),shadowBand);
   cloud=mix(cloud,cloud*vec3(0.48,0.46,0.69),u_eyeAge.w);
   color=mix(color,cloud,body*(layer==0?0.88:0.53));
  }
  vec2 starPixel=floor(ray.xz/(ray.y+0.38)*330.0);float star=step(0.9984,artHash(starPixel));
  color=mix(color,vec3(0.89,0.93,1.0),star*u_eyeAge.w);
 }
 return color;
}
vec3 starfield(vec2 coordinate){
 float time=u_menuScene.y;vec2 screen=u_menuScene.zw;vec2 pixel=floor(coordinate*screen);
 float band=floor(coordinate.y*48.0)/48.0;
 vec3 color=mix(vec3(0.012,0.058,0.103),vec3(0.018,0.113,0.170),band);
 // Fine sparse stars sit on a logical pixel grid, so retina resolution does
 // not inflate them into tiles. Every cell uses a fixed local art seed.
 vec2 cell=floor(pixel/22.0);float seed=artHash(cell+19.0);
 vec2 centre=(cell+vec2(0.14+artHash(cell+3.0)*0.72,0.14+artHash(cell+11.0)*0.72))*22.0;
 centre=floor(centre);vec2 delta=abs(pixel-centre);
 float star=(1.0-step(0.6,max(delta.x,delta.y)))*step(0.80,seed);
 float phase=artHash(cell+73.0)*6.2831853;
 float frequency=0.22+artHash(cell+47.0)*0.53;
 float twinkle=0.30+0.70*(0.5+0.5*sin(time*frequency+phase));
 vec3 starColor=mix(vec3(0.55,0.67,0.91),vec3(0.93,0.97,1.0),seed);
 color=mix(color,starColor,star*twinkle*0.76);
 // Only a few bright stars receive delicate one-pixel cross arms.
 float bright=step(0.991,seed);
 float arms=max((1.0-step(0.6,delta.x))*(1.0-step(2.6,delta.y)),(1.0-step(2.6,delta.x))*(1.0-step(0.6,delta.y)));
 color=mix(color,vec3(0.72,0.83,1.0),arms*bright*twinkle*0.56);
 float glow=(1.0-step(4.6,max(delta.x,delta.y)))*bright;
 color+=vec3(0.018,0.029,0.071)*glow*twinkle;
 // Meteors are infrequent, short-lived diagonal strokes with a tapering
 // fading tail. They never become World actors or consume simulation RNG.
 for(int i=0;i<2;++i){
  float period=13.0+float(i)*9.7;float cycle=mod(time+float(i)*7.3,period);float active=1.0-step(1.15,cycle);
  float run=floor((time+float(i)*7.3)/period);float variation=artHash(vec2(run,float(i)+31.0));
  vec2 direction=normalize(vec2(-0.83,0.56));
  vec2 start=vec2((0.70+variation*0.22)*screen.x,(0.045+float(i)*0.16)*screen.y);
  vec2 head=start+direction*cycle*screen.x*0.30;vec2 offset=pixel-floor(head);
  float along=dot(offset,direction),perpendicular=abs(offset.x*direction.y-offset.y*direction.x);
  float trail=(1.0-step(1.15,perpendicular))*step(-105.0,along)*(1.0-step(0.0,along));
  float fade=pow(max(0.0,1.0+along/105.0),2.1)*sin(min(cycle/1.15,1.0)*3.1415926);
  color+=vec3(0.35,0.53,0.84)*trail*fade*active;
  float tip=(1.0-step(1.6,length(offset)))*active*sin(min(cycle/1.15,1.0)*3.1415926);
  color=mix(color,vec3(0.89,0.96,1.0),tip*0.87);
 }
 return displayColor(color);
}

void main(){
 if(u_menuScene.x>0.5){gl_FragColor=vec4(starfield(v_uv),1.0);return;}
 vec2 p=vec2(v_uv.x*2.0-1.0,1.0-v_uv.y*2.0);
 vec3 ray=normalize(u_cameraForward.xyz+p.x*u_cameraRight.xyz*u_cameraRight.w+p.y*u_cameraUp.xyz*u_cameraUp.w);
 gl_FragColor=vec4(displayColor(illustratedSky(ray)),1.0);
}
