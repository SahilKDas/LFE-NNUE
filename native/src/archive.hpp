#pragma once
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <type_traits>
#include <stdexcept>
namespace life {
struct StateArchive {
  std::iostream& stream; bool reading;
  uint64_t checksum=1469598103934665603ULL;
  void bytes(char* data,size_t size){if(reading)stream.read(data,size);else stream.write(data,size);if(!stream)throw std::runtime_error("Incomplete simulation archive");for(size_t i=0;i<size;++i){checksum^=uint8_t(data[i]);checksum*=1099511628211ULL;}}
  template<class T> void operator()(T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    bytes(reinterpret_cast<char*>(&value),sizeof(value));
  }
  template<class T> void operator()(std::vector<T>& values) {
    uint64_t count=values.size();(*this)(count);
    if(count>8'000'000)throw std::runtime_error("Invalid archive collection size");
    if(reading)values.resize(size_t(count));
    for(auto& value:values)(*this)(value);
  }
  void operator()(std::string& value) {
    uint64_t count=value.size();(*this)(count);
    if(count>1'000'000)throw std::runtime_error("Invalid archive text size");
    if(reading)value.resize(size_t(count));
    bytes(value.data(),size_t(count));
  }
  template<class... T> void fields(T&... values){((*this)(values),...);}
};
}
