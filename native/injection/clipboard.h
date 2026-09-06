#pragma once
#include "common.h"
namespace flow {
struct ClipboardFormat { UINT id; std::vector<std::byte> bytes; };
class Clipboard {
    HWND owner_;
    std::optional<std::vector<ClipboardFormat>> backup_;
    DWORD sequence_=0;
public:
    explicit Clipboard(HWND owner):owner_(owner){}
    Json put(const std::wstring& text);
    Json restore();
    bool recoverable();
    DWORD sequence() const { return sequence_; }
};
}
