#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <WebView2.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;
constexpr unsigned short Port = 4178;
constexpr wchar_t WindowClass[] = L"LifeEngineWindow";
HWND mainWindow = nullptr;
ICoreWebView2Controller* webController = nullptr;
ICoreWebView2* webView = nullptr;
SOCKET serverSocket = INVALID_SOCKET;
std::atomic<bool> running = true;

std::string mime(const fs::path& path) {
  const auto ext = path.extension().string();
  if (ext == ".html") return "text/html; charset=utf-8";
  if (ext == ".js") return "text/javascript; charset=utf-8";
  if (ext == ".css") return "text/css; charset=utf-8";
  if (ext == ".json" || ext == ".map") return "application/json";
  return "application/octet-stream";
}

std::string readFile(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}

fs::path distributionRoot() {
  wchar_t executable[MAX_PATH]{};
  GetModuleFileNameW(nullptr, executable, MAX_PATH);
  auto directory = fs::path(executable).parent_path();
  for (int depth = 0; depth < 5; ++depth) {
    const auto candidate = directory / "dist" / "index.html";
    if (fs::exists(candidate)) return candidate.parent_path();
    directory = directory.parent_path();
  }
  return {};
}

std::string cleanTarget(std::string target) {
  const auto query = target.find('?');
  if (query != std::string::npos) target.resize(query);
  if (target == "/") return "index.html";
  while (!target.empty() && target.front() == '/') target.erase(target.begin());
  if (target.find("..") != std::string::npos) return "index.html";
  return target;
}

void serveClient(SOCKET client, const fs::path& root) {
  std::array<char, 8192> buffer{};
  const int received = recv(client, buffer.data(), int(buffer.size() - 1), 0);
  if (received <= 0) { closesocket(client); return; }
  std::istringstream request(std::string(buffer.data(), received));
  std::string method, target, version;
  request >> method >> target >> version;
  auto path = root / cleanTarget(target);
  if (!fs::exists(path) || fs::is_directory(path)) path = root / "index.html";
  const auto body = readFile(path);
  std::ostringstream response;
  response << "HTTP/1.1 200 OK\r\nContent-Type: " << mime(path)
           << "\r\nContent-Length: " << body.size()
           << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  const auto header = response.str();
  send(client, header.data(), int(header.size()), 0);
  size_t sent = 0;
  while (sent < body.size()) {
    const int count = send(client, body.data() + sent,
      int(std::min<size_t>(body.size() - sent, 1 << 20)), 0);
    if (count <= 0) break;
    sent += size_t(count);
  }
  closesocket(client);
}

void serve(const fs::path root) {
  while (running) {
    const SOCKET client = accept(serverSocket, nullptr, nullptr);
    if (client == INVALID_SOCKET) break;
    serveClient(client, root);
  }
}

template<class Interface, const IID* InterfaceId>
class CallbackBase : public Interface {
  std::atomic<ULONG> references{1};
public:
  virtual ~CallbackBase() = default;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** value) override {
    if (!value) return E_POINTER;
    if (id == IID_IUnknown || id == *InterfaceId) {
      *value = static_cast<Interface*>(this);
      AddRef();
      return S_OK;
    }
    *value = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG remaining = --references;
    if (!remaining) delete this;
    return remaining;
  }
};

class ControllerCreated final : public CallbackBase<
  ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
  &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
public:
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT error, ICoreWebView2Controller* controller) override {
    if (FAILED(error) || !controller) {
      MessageBoxW(mainWindow, L"The embedded browser could not be created.", L"Life Engine", MB_ICONERROR);
      return error;
    }
    webController = controller;
    webController->AddRef();
    webController->get_CoreWebView2(&webView);
    RECT bounds{};
    GetClientRect(mainWindow, &bounds);
    webController->put_Bounds(bounds);
    webView->Navigate(L"http://127.0.0.1:4178/");
    return S_OK;
  }
};

class EnvironmentCreated final : public CallbackBase<
  ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
  &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
public:
  HRESULT STDMETHODCALLTYPE Invoke(HRESULT error, ICoreWebView2Environment* environment) override {
    if (FAILED(error) || !environment) {
      MessageBoxW(mainWindow, L"Microsoft Edge WebView2 Runtime is required.", L"Life Engine", MB_ICONERROR);
      return error;
    }
    auto* callback = new ControllerCreated();
    const HRESULT result = environment->CreateCoreWebView2Controller(mainWindow, callback);
    callback->Release();
    return result;
  }
};

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_SIZE && webController) {
    RECT bounds{};
    GetClientRect(window, &bounds);
    webController->put_Bounds(bounds);
    return 0;
  }
  if (message == WM_DESTROY) {
    if (webView) { webView->Release(); webView = nullptr; }
    if (webController) { webController->Close(); webController->Release(); webController = nullptr; }
    running = false;
    if (serverSocket != INVALID_SOCKET) {
      shutdown(serverSocket, SD_BOTH);
      closesocket(serverSocket);
      serverSocket = INVALID_SOCKET;
    }
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show) {
  const auto root = distributionRoot();
  if (root.empty()) {
    MessageBoxW(nullptr, L"Run npm run build before launching Life Engine.", L"Life Engine", MB_ICONERROR);
    return 1;
  }

  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 2;
  serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  BOOL reuse = TRUE;
  setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(Port);
  if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
    MessageBoxW(nullptr, L"Port 4178 is already in use.", L"Life Engine", MB_ICONERROR);
    WSACleanup();
    return 3;
  }
  listen(serverSocket, SOMAXCONN);
  std::thread server(serve, root);

  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = windowProcedure;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = WindowClass;
  windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&windowClass);
  mainWindow = CreateWindowExW(0, WindowClass, L"Life Engine", WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, 1500, 900, nullptr, nullptr, instance, nullptr);
  if (!mainWindow) return 4;
  ShowWindow(mainWindow, show);

  const HMODULE loader = LoadLibraryW(L"WebView2Loader.dll");
  using CreateEnvironment = HRESULT(STDAPICALLTYPE*)(PCWSTR, PCWSTR,
    ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
  const auto createEnvironment = loader ? reinterpret_cast<CreateEnvironment>(
    GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions")) : nullptr;
  if (!createEnvironment) {
    MessageBoxW(mainWindow, L"WebView2Loader.dll is missing.", L"Life Engine", MB_ICONERROR);
    DestroyWindow(mainWindow);
  } else {
    auto* callback = new EnvironmentCreated();
    createEnvironment(nullptr, nullptr, nullptr, callback);
    callback->Release();
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  running = false;
  if (serverSocket != INVALID_SOCKET) {
    shutdown(serverSocket, SD_BOTH);
    closesocket(serverSocket);
    serverSocket = INVALID_SOCKET;
  }
  if (server.joinable()) server.join();
  if (loader) FreeLibrary(loader);
  CoUninitialize();
  WSACleanup();
  return int(message.wParam);
}
