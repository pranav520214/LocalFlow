#pragma once
#define NOMINMAX
#include <Windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <chrono>
#include <thread>
#include <algorithm>
#include <stdexcept>
#include "vendor/json.hpp"
namespace flow {
using Microsoft::WRL::ComPtr;
using Json = nlohmann::json;
struct Error : std::runtime_error {
    std::string code;
    HRESULT hr;
    explicit Error(std::string c, HRESULT h = S_OK) : std::runtime_error(c), code(std::move(c)), hr(h) {}
};
inline void check(HRESULT hr, const char* code = "UiAutomationUnavailable") { if (FAILED(hr)) throw Error(hr == E_ACCESSDENIED ? "AccessDenied" : code, hr); }
inline std::string utf8(const std::wstring& s) {
    if (s.empty()) return {};
    const auto n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr, nullptr);
    return out;
}
inline std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) throw Error("InvalidText");
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
struct BString {
    BSTR value = nullptr;
    ~BString() { SysFreeString(value); }
    BString() = default;
    BString(const BString&) = delete;
    BString& operator=(const BString&) = delete;
    std::wstring str() const { return value ? std::wstring(value, SysStringLen(value)) : std::wstring{}; }
};
struct Variant {
    VARIANT value;
    Variant() { VariantInit(&value); }
    ~Variant() { VariantClear(&value); }
    Variant(const Variant&) = delete;
    Variant& operator=(const Variant&) = delete;
};
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if(value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
template<class T> ComPtr<T> pattern(IUIAutomationElement* e, PATTERNID id) {
    ComPtr<T> out;
    if (FAILED(e->GetCurrentPatternAs(id, __uuidof(T), reinterpret_cast<void**>(out.GetAddressOf())))) out.Reset();
    return out;
}
inline Json status(const char* name) { return Json{{"status",name}}; }
}
