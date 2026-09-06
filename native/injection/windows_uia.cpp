#include "target.h"
namespace flow {
Automation::Automation(){check(CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&uia_)));ComPtr<IUIAutomation2> options;if(SUCCEEDED(uia_.As(&options))){options->put_ConnectionTimeout(500);options->put_TransactionTimeout(750);}}
ComPtr<IUIAutomationElement> Automation::candidate(IUIAutomationElement* element) {
    ComPtr<IUIAutomationElement> current=element;
    ComPtr<IUIAutomationTreeWalker> walker;check(uia_->get_ControlViewWalker(&walker));
    for(int depth=0;current&&depth<6;++depth){
        int pid=0;check(current->get_CurrentProcessId(&pid));if(static_cast<DWORD>(pid)==GetCurrentProcessId())throw Error("OwnWindow");
        if(editable(current.Get()))return current;
        CONTROLTYPEID type=0;current->get_CurrentControlType(&type);
        if(type==UIA_WindowControlTypeId||type==UIA_DocumentControlTypeId)break;
        ComPtr<IUIAutomationElement> parent;check(walker->GetParentElement(current.Get(),&parent));current=std::move(parent);
    }
    throw Error("TargetNotEditable");
}
ComPtr<IUIAutomationElement> Automation::focused(){ComPtr<IUIAutomationElement> e;check(uia_->GetFocusedElement(&e));if(!e)throw Error("NoTarget");return candidate(e.Get());}
TargetDescriptor Automation::describe(IUIAutomationElement* element){
    TargetDescriptor d;
    int pid=0;check(element->get_CurrentProcessId(&pid));d.pid=static_cast<DWORD>(pid);
    check(element->get_CurrentControlType(&d.control_type));
    BString id,name,cls,framework;
    element->get_CurrentAutomationId(&id.value);element->get_CurrentName(&name.value);element->get_CurrentClassName(&cls.value);element->get_CurrentFrameworkId(&framework.value);
    d.automation_id=utf8(id.str());d.name=utf8(name.str());d.class_name=utf8(cls.str());d.framework=utf8(framework.str());d.runtime_id=runtime_id(element);
    ComPtr<IUIAutomationTreeWalker> walker;check(uia_->get_ControlViewWalker(&walker));
    ComPtr<IUIAutomationElement> current=element;
    HWND top=nullptr;
    for(int depth=0;current&&depth<40;++depth){
        UIA_HWND handle=nullptr;current->get_CurrentNativeWindowHandle(&handle);if(handle)top=GetAncestor(static_cast<HWND>(handle),GA_ROOT);
        CONTROLTYPEID type=0;current->get_CurrentControlType(&type);
        if(type==UIA_DocumentControlTypeId&&d.document_id.empty())d.document_id=runtime_id(current.Get());
        if(top)break;
        ComPtr<IUIAutomationElement> parent;check(walker->GetParentElement(current.Get(),&parent));current=std::move(parent);
    }
    if(!top||!IsWindow(top))throw Error("TargetClosed");
    GetWindowThreadProcessId(top,&d.window_pid);
    if(d.window_pid==GetCurrentProcessId())throw Error("OwnWindow");
    d.hwnd=reinterpret_cast<std::uintptr_t>(top);d.process_started=process_stamp(d.window_pid);
    wchar_t title[1024]{};GetWindowTextW(top,title,1024);d.window_title=utf8(title);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,d.window_pid));
    wchar_t file[32768]{};DWORD length=32768;
    if(process.value&&QueryFullProcessImageNameW(process.value,0,file,&length)){std::wstring s(file,length);d.process_name=utf8(s.substr(s.find_last_of(L"\\/")+1));}
    return d;
}
static ComPtr<IUIAutomationElement> find_runtime(IUIAutomation* uia,IUIAutomationElement* root,const std::vector<int>& ids){
    if(ids.empty())return {};
    Variant v;v.value.vt=VT_ARRAY|VT_I4;v.value.parray=SafeArrayCreateVector(VT_I4,0,static_cast<ULONG>(ids.size()));
    if(!v.value.parray)throw Error("UiAutomationUnavailable",E_OUTOFMEMORY);
    for(LONG i=0;i<static_cast<LONG>(ids.size());++i){auto value=ids[static_cast<size_t>(i)];check(SafeArrayPutElement(v.value.parray,&i,&value));}
    ComPtr<IUIAutomationCondition> cond;check(uia->CreatePropertyCondition(UIA_RuntimeIdPropertyId,v.value,&cond));
    ComPtr<IUIAutomationElement> found;check(root->FindFirst(TreeScope_Subtree,cond.Get(),&found),"TargetUnavailable");return found;
}
ComPtr<IUIAutomationElement> Automation::resolve(const TargetDescriptor& d){
    const auto hwnd=reinterpret_cast<HWND>(d.hwnd);DWORD pid=0;
    if(!IsWindow(hwnd))throw Error("TargetClosed");GetWindowThreadProcessId(hwnd,&pid);
    if(pid!=d.window_pid||process_stamp(pid)!=d.process_started)throw Error("TargetClosed");
    if(pid==GetCurrentProcessId())throw Error("OwnWindow");
    ComPtr<IUIAutomationElement> root;check(uia_->ElementFromHandle(hwnd,&root),"TargetClosed");
    // The exact focused element is the common case; avoid a subtree query.
    ComPtr<IUIAutomationElement> found;
    try{auto e=focused();if(runtime_id(e.Get())==d.runtime_id)found=e;}catch(const Error&){ }
    if(!found)found=find_runtime(uia_.Get(),root.Get(),d.runtime_id);
    if(!found&&!d.automation_id.empty()){
        // Reacquire only within the SAME document instance. Never match a new
        // browser tab/page by title, URL, rectangle, or a generic textbox name.
        auto scope=d.document_id.empty()?root:find_runtime(uia_.Get(),root.Get(),d.document_id);
        if(scope){
            Variant v;v.value.vt=VT_BSTR;v.value.bstrVal=SysAllocString(wide(d.automation_id).c_str());
            ComPtr<IUIAutomationCondition> cond;check(uia_->CreatePropertyCondition(UIA_AutomationIdPropertyId,v.value,&cond));
            ComPtr<IUIAutomationElementArray> matches;check(scope->FindAll(TreeScope_Subtree,cond.Get(),&matches),"TargetUnavailable");
            int count=0;matches->get_Length(&count);if(count==1)check(matches->GetElement(0,&found));
        }
    }
    if(!found)throw Error("TargetUnavailable");
    if(!editable(found.Get()))throw Error("TargetNotEditable");
    BOOL offscreen=TRUE;check(found->get_CurrentIsOffscreen(&offscreen));if(offscreen)throw Error("TargetUnavailable");
    const auto fresh=describe(found.Get());
    if(fresh.hwnd!=d.hwnd||fresh.window_pid!=d.window_pid||fresh.process_started!=d.process_started||fresh.pid!=d.pid||fresh.control_type!=d.control_type||fresh.framework!=d.framework||fresh.class_name!=d.class_name||fresh.document_id!=d.document_id)throw Error("TargetUnavailable");
    if(fresh.runtime_id!=d.runtime_id&&(fresh.automation_id!=d.automation_id||fresh.name!=d.name))throw Error("TargetUnavailable");
    return found;
}
bool Automation::matches_focus(IUIAutomationElement* element,const TargetDescriptor& d){
    if(GetForegroundWindow()!=reinterpret_cast<HWND>(d.hwnd))return false;
    try{auto focus=focused();BOOL same=FALSE;check(uia_->CompareElements(focus.Get(),element,&same));return same&&editable(focus.Get())&&describe(focus.Get()).process_started==d.process_started;}catch(const Error& e){if(e.code=="ProtectedInput")throw;return false;}
}
void Automation::focus(IUIAutomationElement* element,const TargetDescriptor& d){
    const auto hwnd=reinterpret_cast<HWND>(d.hwnd);
    if(IsIconic(hwnd))ShowWindowAsync(hwnd,SW_RESTORE);
    if(GetForegroundWindow()!=hwnd&&!SetForegroundWindow(hwnd))throw Error("FocusFailed");
    check(element->SetFocus(),"FocusFailed");
    for(int attempt=0;attempt<12;++attempt){if(matches_focus(element,d))return;std::this_thread::sleep_for(std::chrono::milliseconds(15));}
    throw Error("FocusFailed");
}
}
