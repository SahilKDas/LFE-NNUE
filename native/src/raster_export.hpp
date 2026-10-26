#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>
#include <stdexcept>

namespace life {
inline void exportRasterPng(const std::filesystem::path& path,int width,int height,std::span<const uint32_t> pixels){
  if(width<=0||height<=0||pixels.size()!=size_t(width)*height)throw std::runtime_error("Invalid raster export dimensions");
  std::vector<uint8_t> image;image.reserve(size_t(height)*(1+size_t(width)*3));
  for(int y=0;y<height;++y){image.push_back(0);for(int x=0;x<width;++x){const uint32_t pixel=pixels[size_t(y)*width+x];image.push_back(uint8_t(pixel>>16));image.push_back(uint8_t(pixel>>8));image.push_back(uint8_t(pixel));}}
  std::vector<uint8_t> compressed{0x78,0x01};uint32_t a=1,b=0;
  for(size_t begin=0;begin<image.size();){const uint16_t length=uint16_t(std::min<size_t>(65535,image.size()-begin));compressed.push_back(begin+length==image.size()?1:0);compressed.push_back(uint8_t(length));compressed.push_back(uint8_t(length>>8));compressed.push_back(uint8_t(~length));compressed.push_back(uint8_t((~length)>>8));for(size_t i=begin;i<begin+length;++i){compressed.push_back(image[i]);a=(a+image[i])%65521;b=(b+a)%65521;}begin+=length;}
  const auto word=[](std::vector<uint8_t>& out,uint32_t value){for(int shift:{24,16,8,0})out.push_back(uint8_t(value>>shift));};word(compressed,(b<<16)|a);
  std::vector<uint8_t> png{137,80,78,71,13,10,26,10};
  const auto chunk=[&](const char* type,std::span<const uint8_t> bytes){word(png,uint32_t(bytes.size()));const size_t begin=png.size();for(int i=0;i<4;++i)png.push_back(uint8_t(type[i]));png.insert(png.end(),bytes.begin(),bytes.end());uint32_t crc=0xffffffff;for(size_t index=begin;index<png.size();++index){crc^=png[index];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int(crc&1)));}word(png,~crc);};
  std::vector<uint8_t> header;word(header,uint32_t(width));word(header,uint32_t(height));header.insert(header.end(),{8,2,0,0,0});chunk("IHDR",header);chunk("IDAT",compressed);chunk("IEND",{});
  std::ofstream file(path,std::ios::binary|std::ios::trunc);file.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size()));if(!file)throw std::runtime_error("Unable to export raster PNG");
}
}
