#include <sonnheide/native_platform.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv){
 try{
  if(argc!=2)throw std::runtime_error("Pass project resource root");
  auto root=std::filesystem::path(std::u8string(argv[1],argv[1]+std::char_traits<char>::length(argv[1])));
  // Original 2x2 fixture: red and half-alpha green above blue and transparent white.
  const unsigned char png[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,21,73,68,65,84,120,156,99,248,207,192,240,31,8,27,24,128,52,8,48,0,0,67,211,8,121,201,21,28,15,0,0,0,0,73,69,78,68,174,66,96,130};
  auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  auto directory=std::filesystem::temp_directory_path()/std::filesystem::path(std::u8string(u8"sonnheide-\u56fe\u50cf-")+std::u8string(stamp.begin(),stamp.end()));
  std::filesystem::create_directories(directory);
  struct Remove {std::filesystem::path p;~Remove(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{directory};
  auto fixture=directory/std::filesystem::path(u8"\u4e0a\u4e0b.png");
  {std::ofstream f(fixture,std::ios::binary);f.write(reinterpret_cast<const char*>(png),sizeof(png));}
  auto image=sonnheide::native::decode_bimg_image(fixture);
  require(image.width==2&&image.height==2,"Fixture size");
  const std::vector<std::uint8_t> expected={255,0,0,255,0,255,0,128,0,0,255,255,255,255,255,0};
  require(image.rgba==expected,"Decoder must preserve top-down orientation and straight RGBA");
  sonnheide::native::UnicodeFileInterface files;
  auto utf8=fixture.u8string();auto h=files.Open(std::string(utf8.begin(),utf8.end()));require(h!=0,"Unicode Rml file open");
  require(files.Length(h)==sizeof(png),"Unicode Rml file length");unsigned char signature[8];require(files.Read(signature,8,h)==8&&signature[0]==137,"Unicode Rml file read");require(files.Seek(h,0,SEEK_SET)&&files.Tell(h)==0,"Unicode Rml seek/tell");files.Close(h);
  for(int i=1;i<=5;++i){auto decoded=sonnheide::native::decode_bimg_image(root/("assets/source/ui/paintings/painting-0"+std::to_string(i)+".jpg"));require(decoded.width>0&&decoded.height>0&&decoded.rgba.size()==std::size_t(decoded.width)*decoded.height*4,"Original JPEG decode");}
  auto logo=sonnheide::native::decode_bimg_image(root/"assets/source/ui/branding/sonnreich_logo_transparent_fullsize.png");require(logo.width>0&&logo.height>0,"Original PNG decode");
  bool failed=false;try{sonnheide::native::decode_bimg_image(directory/"missing.png");}catch(const std::exception&){failed=true;}require(failed,"Missing image must fail");
  std::cout<<"bimg JPEG/PNG original assets, orientation, alpha, Unicode files and missing-image checks passed\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
