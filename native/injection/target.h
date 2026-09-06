#pragma once
#include "common.h"
namespace flow {
struct TargetDescriptor {
    DWORD pid = 0, window_pid = 0;
    std::uint64_t process_started = 0;
    std::uintptr_t hwnd = 0;
    int control_type = 0;
    std::string process_name, window_title, automation_id, name, class_name, framework;
    std::vector<int> runtime_id, document_id;
};
void to_json(Json& j, const TargetDescriptor& d);
void from_json(const Json& j, TargetDescriptor& d);
std::uint64_t process_stamp(DWORD pid);
std::vector<int> runtime_id(IUIAutomationElement* element);
bool password(IUIAutomationElement* element);
bool editable(IUIAutomationElement* element);
class Automation {
    ComPtr<IUIAutomation> uia_;
public:
    Automation();
    IUIAutomation* api() const { return uia_.Get(); }
    ComPtr<IUIAutomationElement> focused();
    TargetDescriptor describe(IUIAutomationElement* element);
    ComPtr<IUIAutomationElement> resolve(const TargetDescriptor& descriptor);
    bool matches_focus(IUIAutomationElement* element, const TargetDescriptor& descriptor);
    void focus(IUIAutomationElement* element, const TargetDescriptor& descriptor);
    ComPtr<IUIAutomationElement> candidate(IUIAutomationElement* element);
};
struct TextContext {
    std::wstring before, prefix, suffix;
    bool readable = false, cursor_known = false;
};
TextContext text_context(IUIAutomationElement* element);
std::wstring spaced(const std::wstring& text, const TextContext& context);
}
