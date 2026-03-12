#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include "com.cpp"
#include "FunctionTraits.cpp"
#include "nnwww.hpp"

#define CHECK_FAILURE(r)         \
  do {                           \
    HRESULT hr = r;              \
    if (FAILED(hr)) [[unlikely]] \
      return hr;                 \
  } while (0)

template <typename Base, typename Fn, typename... Args>
class InvokeHelper : public com::BaseClass<Base> {
public:
  InvokeHelper(const Fn& callback) : callback_(callback) {}
  InvokeHelper(Fn&& callback) : callback_(std::move(callback)) {}

  auto Invoke(Args... args) -> std::invoke_result_t<Fn, Args...> override {
    return std::invoke(callback_, std::forward<Args>(args)...);
  }

private:
  Fn callback_;
};

template <typename Base, typename Fn> struct UnwrapArguments {
  using Forward = decltype([] {
    using Lambda_Args = std::conditional_t<
      std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>,
      typename FunctionTraits<Fn>::argsType,
      typename FunctionTraits<Fn>::memFuncArgsType>;
    // fuck...
    return []<std::size_t... Idx>(std::index_sequence<Idx...>) {
      return std::type_identity<InvokeHelper<Base, Fn, std::tuple_element_t<Idx, Lambda_Args>...>>{};
    }(std::make_index_sequence<std::tuple_size_v<Lambda_Args>>());
  }())::type;
};

template <class Base, class Fn>
class Webview2ComPtr : public UnwrapArguments<Base, Fn>::Forward {
public:
  explicit Webview2ComPtr(Fn callback) : UnwrapArguments<Base, Fn>::Forward(std::move(callback)) {}
  Webview2ComPtr(Webview2ComPtr&&) = delete;
};

template <typename Base, typename Fn> auto webview2ComPtrMake(Fn&& fn) {
  return com::ptrMake<Webview2ComPtr<Base, std::decay_t<Fn>>>(std::forward<Fn>(fn));
}

static const auto www = reinterpret_cast<const NNWww*>(__www_start);

constexpr auto APP_WIDTH                              = 720;
constexpr auto APP_HEIGHT                             = 540;
constexpr auto APP_TITLE                              = L"";
constexpr auto APP_CLASS_NAME                         = L"NNWebview2Class";
constexpr auto APP_USER_DATA_FOLDER                   = L"cache";
constexpr auto APP_BROWSER_EXECUTABLE_FOLDER          = nullptr;
constexpr auto APP_CUSTOM_RESOURCE_REQUEST_URI_HEADER = L"nnwebview2://";
constexpr auto APP_CUSTOM_RESOURCE_REQUEST_URI_FITER  = L"nnwebview2://*";
constexpr auto APP_CUSTOM_SCHEME                      = L"nnwebview2";
constexpr auto APP_ALLOWED_ORIGIN                     = L"*";

auto window              = HWND{};
auto webview2Options     = com::Ptr<ICoreWebView2EnvironmentOptions>{};
auto webview2Environment = com::Ptr<ICoreWebView2Environment>{};
auto webview2_28         = com::Ptr<ICoreWebView2_28>{};
auto webview2Controller  = com::Ptr<ICoreWebView2Controller>{};
auto webview2Settings    = com::Ptr<ICoreWebView2Settings>{};

template <typename T, typename... Args> auto makeof(Args&&... args) {
  return T(std::forward<Args>(args)...);
};

auto istreamMake() {
  auto stream = makeof<IStream*>(nullptr);
  return SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) ? stream : nullptr;
}

auto extname(std::wstring_view s) {
  return s.substr(s.find_last_of(L"."));
}

auto wstringToString(const std::wstring_view ws) {
  auto result = std::string{};
  auto size   = ws.length();
  result.resize(size);
  wcstombs_s(nullptr, result.data(), size + 1, ws.data(), _TRUNCATE);
  return result;
}

auto stringToWstring(const std::string_view s) {
  auto result = std::wstring{};
  auto size   = s.length();
  result.resize(size);
  mbstowcs_s(nullptr, result.data(), size + 1, s.data(), _TRUNCATE);
  return result;
}

auto findRes(std::wstring_view uri) -> std::tuple<const uint8_t*, size_t> {
  for (auto i = size_t{}; i < www->count; ++i) {
    auto filePath  = uri.substr(13, uri.length() - 14);
    auto sfilePath = wstringToString(filePath);
    auto name      = std::string{www->names[i]};
    if (sfilePath == name)
      return std::tuple{www->data[i], i};
  }
  return {};
}

auto webview2ResourceRequestedProc(LPWSTR uri, ICoreWebView2WebResourceResponse* res) {
  static const auto mime_type = std::unordered_map<std::wstring, std::wstring>{
    {L".xml", L"text/xml"},
    {L".html", L"text/html"},
    {L".js", L"text/javascript"},
    {L".css", L"text/css"},
    {L".json", L"application/json"},
    {L".png", L"image/png"},
    {L".jpg", L"image/jpeg"},
    {L".jpeg", L"image/jpeg"},
    {L".gif", L"image/gif"},
    {L".svg", L"image/svg+xml"},
    {L".ico", L"image/x-icon"},
    {L".webp", L"image/webp"},
    {L".ttf", L"font/ttf"},
    {L".woff", L"font/woff"},
    {L".woff2", L"font/woff2"},
    {L".mp3", L"audio/mpeg"},
    {L".wav", L"audio/wav"},
    {L".mp4", L"video/mp4"},
    {L".h264", L"video/H264"},
    {L".H264", L"video/H264"},
    {L".webm", L"video/webm"},
    {L".pdf", L"application/pdf"},
    {L".zip", L"application/zip"}
  };
  auto extView = extname(uri);
  auto ext     = extView.substr(0, extView.length() - 1);
  auto key     = std::wstring(ext.data(), ext.length());
  auto it      = mime_type.find(key);
  if (it == mime_type.end())
    return;

  auto [contentAddress, idx] = findRes(uri);
  if (!contentAddress || idx < 0 || idx >= www->count)
    return;

  auto stream = istreamMake();
  stream->Write(reinterpret_cast<const char*>(contentAddress), www->bytes[idx], nullptr);

  res->put_Content(stream);
  auto headers = makeof<ICoreWebView2HttpResponseHeaders*>();
  res->get_Headers(&headers);
  headers->AppendHeader(L"Content-Type", it->second.c_str());
}

/* master function */
auto webview2Master() {
  {
    auto handler = webview2ComPtrMake<ICoreWebView2NavigationCompletedEventHandler>(
      [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) {
        ShowWindow(window, SW_SHOWDEFAULT);
        CHECK_FAILURE(webview2Controller->put_IsVisible(true));
        return S_OK;
      }
    );
    auto token = EventRegistrationToken{};
    CHECK_FAILURE(webview2_28->add_NavigationCompleted(handler.get(), &token));
  }
  {
    auto token = EventRegistrationToken{};
    auto hr    = webview2_28->AddWebResourceRequestedFilterWithRequestSourceKinds(
      APP_CUSTOM_RESOURCE_REQUEST_URI_FITER,
      COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL,
      COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_ALL
    );
    if (FAILED(hr)) [[unlikely]] {
      webview2_28->remove_WebResourceRequested(token);
      return hr;
    }
    auto handler = webview2ComPtrMake<ICoreWebView2WebResourceRequestedEventHandler>(
      [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
        auto request = com::Ptr<ICoreWebView2WebResourceRequest>{};
        CHECK_FAILURE(args->get_Request(&request));
        auto uri = LPWSTR{};
        CHECK_FAILURE(request->get_Uri(&uri));
        auto environment = com::Ptr<ICoreWebView2Environment>{};
        auto webview2    = com::Ptr<ICoreWebView2_2>{};
        auto response    = com::Ptr<ICoreWebView2WebResourceResponse>{};
        CHECK_FAILURE(webview2_28->QueryInterface(IID_PPV_ARGS(&webview2)));
        CHECK_FAILURE(webview2_28->get_Environment(&environment));
        CHECK_FAILURE(environment->CreateWebResourceResponse(
          nullptr,
          200,
          L"OK",
          nullptr,
          &response
        ));
        webview2ResourceRequestedProc(uri, response.get());
        CHECK_FAILURE(args->put_Response(response.get()));
        return S_OK;
      }
    );
    CHECK_FAILURE(webview2_28->add_WebResourceRequested(handler.get(), &token));
  }
  auto base = std::wstring{APP_CUSTOM_RESOURCE_REQUEST_URI_HEADER};
  base += stringToWstring(www->entry);
  CHECK_FAILURE(webview2_28->Navigate(base.c_str()));
  return S_OK;
}

auto webview2InitSettings() {
  webview2Settings->put_IsZoomControlEnabled(false);
  webview2Settings->put_AreDefaultScriptDialogsEnabled(false);
  webview2Settings->put_AreDefaultContextMenusEnabled(false);
  webview2Settings->put_IsScriptEnabled(true);
  webview2Settings->put_IsWebMessageEnabled(true);
  webview2Settings->put_AreDefaultScriptDialogsEnabled(true);
  webview2Settings->put_AreHostObjectsAllowed(true);
#ifdef DEBUG
  webview2Settings->put_IsStatusBarEnabled(true);
  webview2Settings->put_AreDevToolsEnabled(true);
  webview2Settings->put_IsBuiltInErrorPageEnabled(true);
#else
  webview2Settings->put_IsStatusBarEnabled(false);
  webview2Settings->put_AreDevToolsEnabled(false);
  webview2Settings->put_IsBuiltInErrorPageEnabled(false);
#endif
}

auto webview2WindowResize() {
  auto hr = S_OK;
  if (webview2Controller) {
    auto bounds = RECT{};
    GetClientRect(window, &bounds);
    hr = webview2Controller->put_Bounds(bounds);
  }
  return hr;
}

auto webview2ContrInvoke(HRESULT, ICoreWebView2Controller* controller) {
  if (!controller)
    return S_OK;
  webview2Controller = controller;
  CHECK_FAILURE(controller->get_CoreWebView2(reinterpret_cast<ICoreWebView2**>(&webview2_28)));
  CHECK_FAILURE(webview2_28->get_Settings(&webview2Settings));
  webview2InitSettings();
  auto bounds = RECT{};
  GetClientRect(window, &bounds);
  webview2Controller->put_Bounds(bounds);
  return webview2Master();
}

auto webview2EnvInvoke(HRESULT, ICoreWebView2Environment* env) {
  if (env) {
    webview2Environment = env;
    CHECK_FAILURE(env->CreateCoreWebView2Controller(
      window,
      webview2ComPtrMake<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(webview2ContrInvoke).get()
    ));
  }
  return S_OK;
}

/* lanuch function */
auto webview2CoreLanuch() {
  auto options4 = com::Ptr<ICoreWebView2EnvironmentOptions4>{};
  if (webview2Options->QueryInterface(__uuidof(ICoreWebView2EnvironmentOptions4), (void**)&options4) == S_OK) {
    auto treatAsSecure         = true;
    auto hasAuthorityComponent = true;
    auto allowedOrigins        = std::array{APP_ALLOWED_ORIGIN};
    auto reg                   = com::ptrMake<CoreWebView2CustomSchemeRegistration>(APP_CUSTOM_SCHEME);
    CHECK_FAILURE(reg->put_TreatAsSecure(treatAsSecure));
    CHECK_FAILURE(reg->SetAllowedOrigins(1, allowedOrigins.data()));
    CHECK_FAILURE(reg->put_HasAuthorityComponent(hasAuthorityComponent));
    auto regs = std::array<ICoreWebView2CustomSchemeRegistration*, 1>{reg.get()};
    CHECK_FAILURE(options4->SetCustomSchemeRegistrations(1, regs.data()));
  }
  return CreateCoreWebView2EnvironmentWithOptions(
    APP_BROWSER_EXECUTABLE_FOLDER,
    APP_USER_DATA_FOLDER,
    webview2Options.get(),
    webview2ComPtrMake<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(webview2EnvInvoke).get()
  );
}

auto eventLoop(HWND wnd, UINT msg, WPARAM wpm, LPARAM lpm) -> LRESULT {
  switch (msg) {
  case WM_SIZE:
    webview2WindowResize();
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(wnd, msg, wpm, lpm);
  }
  return 0;
}

auto main() -> int {
  auto module = GetModuleHandleW(NULL);

  auto wc = WNDCLASSEXW{
    .cbSize        = sizeof(WNDCLASSEX),
    .style         = CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc   = eventLoop,
    .cbClsExtra    = 0,
    .cbWndExtra    = sizeof(void*),
    .hInstance     = module,
    .hIcon         = LoadIcon(module, IDI_APPLICATION),
    .hCursor       = LoadCursor(nullptr, IDC_ARROW),
    .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
    .lpszMenuName  = NULL,
    .lpszClassName = APP_CLASS_NAME,
    .hIconSm       = LoadIcon(module, IDI_APPLICATION)
  };
  if (!RegisterClassExW(&wc))
    return GetLastError();

  window = CreateWindowExW(
    0,
    APP_CLASS_NAME,
    APP_TITLE,
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    APP_WIDTH,
    APP_HEIGHT,
    nullptr,
    nullptr,
    module,
    nullptr
  );
  if (!window)
    return GetLastError();

  ShowWindow(window, SW_HIDE);

  auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr))
    return hr;

  webview2Options = com::ptrMake<CoreWebView2EnvironmentOptions>();
  hr              = webview2CoreLanuch();
  if (FAILED(hr))
    return hr;

  auto msg = MSG{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  CoUninitialize();
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
  DestroyWindow(window);
  return msg.wParam;
}
