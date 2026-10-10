// Both zero-height land and water read the same admitted management overview
// and the same sparse exact64x64 microcolumn atlas. No colour-generated coast.
SAMPLER2D(s_surfaceInfo,4);
SAMPLER2D(s_fineAtlas,5);
SAMPLER2D(s_finePages,6);
uniform vec4 u_waterDomain;
uniform vec4 u_fineDomain;
uniform vec4 u_surfaceMode;
// Packed descriptor: R=kind*8+theme; G=dry_count low8;
// B=dry_count high5 + wet_kind*32. Counts 1/4096 remain nonzero.
vec4 decodeSurfaceInfo(vec4 encoded){
 vec3 code=floor(encoded.rgb*255.0+0.5);
 return vec4(floor(code.r/8.0)/255.0,mod(code.r,8.0)/255.0,(code.g+mod(code.b,32.0)*256.0)/4096.0,floor(code.b/32.0)/255.0);
}
vec4 overviewSurfaceInfo(vec2 management){
 if(management.x<0.0||management.y<0.0||management.x>=u_waterDomain.z/u_surfaceMode.y||management.y>=u_waterDomain.w/u_surfaceMode.y)return vec4(3.0/255.0,0.0,0.0,0.0);
 return decodeSurfaceInfo(texture2D(s_surfaceInfo,(management+0.5)*u_surfaceMode.y/u_waterDomain.zw));
}
vec4 surfaceInfo(vec2 cameraRelativeXZ){
 vec2 local=cameraRelativeXZ-u_waterDomain.xy;float cell=u_surfaceMode.y;
 if(local.x<0.0||local.y<0.0||local.x>=u_waterDomain.z||local.y>=u_waterDomain.w)return vec4(3.0/255.0,0.0,0.0,0.0);
 vec2 management=floor(local/cell);
 vec4 info=overviewSurfaceInfo(management);
 // Farther than one management cell per pixel, area coverage is averaged
 // across the actual footprint. A small island is a colour contribution,
 // not silently discarded by sampling a water-only representative centre.
 vec2 footprintX=dFdx(cameraRelativeXZ),footprintY=dFdy(cameraRelativeXZ);
 float footprint=max(length(footprintX),length(footprintY));
 if(footprint>cell*0.75){
  vec4 chosen=info;float total=0.0,bestDry=-1.0,bestWet=-1.0,wetKind=info.a;
  for(int y=0;y<2;++y)for(int x=0;x<2;++x){
   vec2 offset=(float(x)-0.5)*footprintX+(float(y)-0.5)*footprintY;
   vec2 sampleCell=floor((local+offset)/cell);vec4 sampleInfo=overviewSurfaceInfo(sampleCell);total+=sampleInfo.b;
   if(sampleInfo.b>bestDry){bestDry=sampleInfo.b;chosen=sampleInfo;}
   if(1.0-sampleInfo.b>bestWet){bestWet=1.0-sampleInfo.b;wetKind=sampleInfo.a;}
  }
  info=vec4(chosen.r,chosen.g,total*0.25,wetKind);
 }
 vec2 nearCell=floor((cameraRelativeXZ-u_fineDomain.xy)/cell);
 if(footprint<=cell*0.75&&nearCell.x>=0.0&&nearCell.y>=0.0&&nearCell.x<u_fineDomain.z&&nearCell.y<u_fineDomain.z){
  vec4 page=texture2D(s_finePages,(nearCell+0.5)/u_fineDomain.z);
  float slot=floor(page.r*255.0+0.5)+floor(page.g*255.0+0.5)*256.0-1.0;
  if(slot>=0.0){vec2 tile=vec2(mod(slot,32.0),floor(slot/32.0));vec2 micro=(floor(fract((cameraRelativeXZ-u_fineDomain.xy)/cell)*64.0)+0.5)/64.0;info=decodeSurfaceInfo(texture2D(s_fineAtlas,(tile+micro)/32.0));}
 }
 return info;
}
