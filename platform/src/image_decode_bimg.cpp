#include <sonnheide/native_platform.hpp>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <fstream>
#include <memory>
#include <stdexcept>
namespace sonnheide::native {
ImagePixels decode_bimg_image(const std::filesystem::path& path) {
 std::ifstream stream(path,std::ios::binary|std::ios::ate);
 if(!stream)throw std::runtime_error("Cannot read image");
 auto size=stream.tellg();
 if(size<=0 || size>64*1024*1024)throw std::runtime_error("Invalid or oversized image file");
 std::vector<std::uint8_t> source(static_cast<std::size_t>(size));stream.seekg(0);stream.read(reinterpret_cast<char*>(source.data()),size);
 if(!stream)throw std::runtime_error("Incomplete image file");
 bx::DefaultAllocator allocator;
 std::unique_ptr<bimg::ImageContainer,void(*)(bimg::ImageContainer*)> image(bimg::imageParse(&allocator,source.data(),std::uint32_t(source.size()),bimg::TextureFormat::RGBA8),bimg::imageFree);
 if(!image || image->m_width==0 || image->m_height==0 || image->m_width>16384 || image->m_height>16384 || image->m_cubeMap || image->m_depth>1 || image->m_numLayers!=1)throw std::runtime_error("Cannot decode 2D image");
 bimg::ImageMip mip;
 if(!bimg::imageGetRawData(*image,0,0,image->m_data,image->m_size,mip) || mip.m_format!=bimg::TextureFormat::RGBA8)throw std::runtime_error("Invalid decoded RGBA image");
 ImagePixels out;out.width=int(mip.m_width);out.height=int(mip.m_height);
 const auto expected=std::size_t(out.width)*out.height*4;
 if(mip.m_size<expected)throw std::runtime_error("Truncated decoded image");
 out.rgba.assign(mip.m_data,mip.m_data+expected);
 return out;
}
#ifdef _WIN32
ImagePixels load_image(const std::filesystem::path& path){return decode_bimg_image(path);}
#endif
}
