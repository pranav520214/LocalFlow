#include "focused.h"
namespace flow {
namespace {
bool held_modifiers(){for(const auto key:{VK_CONTROL,VK_SHIFT,VK_MENU,VK_LWIN,VK_RWIN})if(GetAsyncKeyState(key)&0x8000)return true;return false;}
void release_wait(){for(int i=0;i<10&&held_modifiers();++i)std::this_thread::sleep_for(std::chrono::milliseconds(15));if(held_modifiers())throw Error("ModifierHeld");}
}
Json direct_insert(Automation& automation,const TargetDescriptor& target,const std::wstring& text){
    auto element=automation.resolve(target);
    // ValuePattern replaces content, so it is allowed ONLY for an empty native
    // Win32 Edit. Browser/react/document editors always use cursor-aware paste.
    if(target.control_type!=UIA_EditControlTypeId||target.class_name!="Edit"||target.framework!="Win32")return status("Unsupported");
    auto value=pattern<IUIAutomationValuePattern>(element.Get(),UIA_ValuePatternId);if(!value)return status("Unsupported");
    BString before;check(value->get_CurrentValue(&before.value));if(!before.str().empty())return status("Unsupported");
    automation.focus(element.Get(),target);
    BString recheck;check(value->get_CurrentValue(&recheck.value));if(!recheck.str().empty())return status("Unsupported");
    if(!automation.matches_focus(element.Get(),target))throw Error("FocusFailed");
    BSTR bstr=SysAllocString(text.c_str());if(!bstr)throw Error("ClipboardFailed");
    const auto hr=value->SetValue(bstr);SysFreeString(bstr);
    // Once a mutating call has been issued, failure is ambiguous: never paste
    // as a fallback. The provider may have changed text before returning error.
    if(FAILED(hr))return Json{{"status","Unconfirmed"},{"code","BackendFailed"},{"backend","UIAutomation"}};
    return Json{{"status","Inserted"},{"backend","UIAutomation"}};
}
Json paste_insert(Automation& automation,Clipboard& clipboard,const TargetDescriptor& target,const std::wstring& text){
    auto element=automation.resolve(target);automation.focus(element.Get(),target);release_wait();
    const auto context=text_context(element.Get());const auto insertion=spaced(text,context);
    clipboard.put(insertion);
    try{if(!automation.matches_focus(element.Get(),target))throw Error("FocusFailed");if(held_modifiers())throw Error("ModifierHeld");}
    catch(...){try{clipboard.restore();}catch(const Error&){}throw;}
    INPUT keys[4]{};
    for(auto& key:keys)key.type=INPUT_KEYBOARD;
    keys[0].ki.wVk=VK_CONTROL;keys[1].ki.wVk='V';keys[2].ki.wVk='V';keys[2].ki.dwFlags=KEYEVENTF_KEYUP;keys[3].ki.wVk=VK_CONTROL;keys[3].ki.dwFlags=KEYEVENTF_KEYUP;
    const auto count=SendInput(4,keys,sizeof(INPUT));
    if(count!=4){
        // Release only keys introduced by this attempt. Never repeat paste.
        INPUT release[2]={keys[2],keys[3]};if(count>0)SendInput(2,release,sizeof(INPUT));
        return Json{{"status","Unconfirmed"},{"code","AccessDenied"},{"backend","ClipboardPaste"},{"clipboardRecoverable",clipboard.recoverable()}};
    }
    const auto expected=context.prefix+insertion+context.suffix;
    bool verified=false;
    for(int attempt=0;attempt<24;++attempt){
        try{
            const auto after=text_context(element.Get());
            if(context.readable&&after.readable){
                if(context.cursor_known)verified=after.before==expected;
                else verified=after.before!=context.before&&after.before.find(insertion)!=std::wstring::npos;
            }
        }catch(const Error&){break;}
        if(verified)break;std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    if(!verified)return Json{{"status","Unconfirmed"},{"backend","ClipboardPaste"},{"clipboardRecoverable",clipboard.recoverable()}};
    Json restored;
    try{restored=clipboard.restore();}catch(const Error&){restored=status("RestorePending");}
    return Json{{"status","Inserted"},{"backend","ClipboardPaste"},{"clipboard",restored["status"]},{"clipboardRecoverable",clipboard.recoverable()}};
}
}
