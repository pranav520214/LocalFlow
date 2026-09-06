#pragma once
#include "target.h"
#include "clipboard.h"
namespace flow {
Json direct_insert(Automation& automation,const TargetDescriptor& target,const std::wstring& text);
Json paste_insert(Automation& automation,Clipboard& clipboard,const TargetDescriptor& target,const std::wstring& text);
}
