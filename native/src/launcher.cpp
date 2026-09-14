#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs=std::filesystem;
constexpr unsigned short Port=4178;

std::string mime(const fs::path&path){auto ext=path.extension().string();if(ext==".html")return"text/html; charset=utf-8";if(ext==".js")return"text/javascript; charset=utf-8";if(ext==".css")return"text/css; charset=utf-8";if(ext==".json")return"application/json";if(ext==".map")return"application/json";return"application/octet-stream";}
std::string readFile(const fs::path&path){std::ifstream stream(path,std::ios::binary);return{std::istreambuf_iterator<char>(stream),{}};}
fs::path distributionRoot(){wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);auto directory=fs::path(executable).parent_path();for(int depth=0;depth<5;depth++){auto candidate=directory/"dist"/"index.html";if(fs::exists(candidate))return candidate.parent_path();directory=directory.parent_path();}return{};}
std::string cleanTarget(std::string target){auto query=target.find('?');if(query!=std::string::npos)target.resize(query);if(target=="/")return"index.html";while(!target.empty()&&target.front()=='/')target.erase(target.begin());if(target.find("..")!=std::string::npos)return"index.html";return target;}
void serveClient(SOCKET client,const fs::path&root){std::array<char,8192>buffer{};int received=recv(client,buffer.data(),int(buffer.size()-1),0);if(received<=0){closesocket(client);return;}std::istringstream request(std::string(buffer.data(),received));std::string method,target,version;request>>method>>target>>version;auto path=root/cleanTarget(target);if(!fs::exists(path)||fs::is_directory(path))path=root/"index.html";auto body=readFile(path);std::ostringstream response;response<<"HTTP/1.1 200 OK\r\nContent-Type: "<<mime(path)<<"\r\nContent-Length: "<<body.size()<<"\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";auto header=response.str();send(client,header.data(),int(header.size()),0);size_t sent=0;while(sent<body.size()){int count=send(client,body.data()+sent,int(std::min<size_t>(body.size()-sent,1<<20)),0);if(count<=0)break;sent+=size_t(count);}closesocket(client);}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int){auto root=distributionRoot();if(root.empty()){MessageBoxW(nullptr,L"Run npm run build before launching the native compatibility shell.",L"Life Engine",MB_ICONERROR);return 1;}WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)return 2;SOCKET server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);BOOL reuse=TRUE;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(Port);if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))==SOCKET_ERROR){MessageBoxW(nullptr,L"Port 4178 is already in use.",L"Life Engine",MB_ICONERROR);return 3;}listen(server,SOMAXCONN);ShellExecuteW(nullptr,L"open",L"http://127.0.0.1:4178/",nullptr,nullptr,SW_SHOWNORMAL);for(;;){SOCKET client=accept(server,nullptr,nullptr);if(client==INVALID_SOCKET)break;serveClient(client,root);}closesocket(server);WSACleanup();return 0;}
