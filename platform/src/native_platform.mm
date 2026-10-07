#include <sonnheide/native_platform.hpp>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdexcept>
#include <cmath>
#include <iostream>
namespace sonnheide::native {
ImagePixels load_image(const std::filesystem::path& path) {
 @autoreleasepool {
  NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
  CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,nullptr);
  if(!source) throw std::runtime_error("Cannot decode image: "+path.string());
  CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr); CFRelease(source);
  if(!image) throw std::runtime_error("Invalid image: "+path.string());
  ImagePixels out;out.width=int(CGImageGetWidth(image));out.height=int(CGImageGetHeight(image));
  if(out.width<=0 || out.height<=0 || out.width>16384 || out.height>16384){CGImageRelease(image);throw std::runtime_error("Invalid image size");}
  out.rgba.resize(std::size_t(out.width)*out.height*4);
  CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
  CGContextRef canvas=CGBitmapContextCreate(out.rgba.data(),out.width,out.height,8,out.width*4,colors,CGBitmapInfo(kCGImageAlphaPremultipliedLast)|CGBitmapInfo(kCGBitmapByteOrder32Big));
  CGColorSpaceRelease(colors);
  if(!canvas){CGImageRelease(image);throw std::runtime_error("Cannot allocate image canvas");}
  CGContextDrawImage(canvas,CGRectMake(0,0,out.width,out.height),image);CGContextRelease(canvas);CGImageRelease(image);
  // ImageIO gives premultiplied bytes; the UI shader blends straight alpha.
  for(std::size_t i=0;i<out.rgba.size();i+=4)if(out.rgba[i+3]>0 && out.rgba[i+3]<255)
   for(int c=0;c<3;++c)out.rgba[i+c]=std::uint8_t(std::min(255,int(out.rgba[i+c])*255/int(out.rgba[i+3])));
  return out;
 }
}
}
