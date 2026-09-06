#include "target.h"
namespace flow {
void to_json(Json& j, const TargetDescriptor& d) {
    j=Json{{"process_id",d.pid},{"window_pid",d.window_pid},{"process_started",d.process_started},{"hwnd",d.hwnd},{"control_type",d.control_type},{"process_name",d.process_name},{"window_title",d.window_title},{"automation_id",d.automation_id},{"name",d.name},{"class_name",d.class_name},{"framework",d.framework},{"runtime_id",d.runtime_id},{"document_id",d.document_id}};
}
void from_json(const Json& j, TargetDescriptor& d) {
    j.at("process_id").get_to(d.pid);j.at("window_pid").get_to(d.window_pid);j.at("process_started").get_to(d.process_started);j.at("hwnd").get_to(d.hwnd);j.at("control_type").get_to(d.control_type);j.at("process_name").get_to(d.process_name);j.at("window_title").get_to(d.window_title);j.at("automation_id").get_to(d.automation_id);j.at("name").get_to(d.name);j.at("class_name").get_to(d.class_name);j.at("framework").get_to(d.framework);j.at("runtime_id").get_to(d.runtime_id);j.at("document_id").get_to(d.document_id);
}
std::uint64_t process_stamp(DWORD pid) {
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    if(!process.value) throw Error(GetLastError()==ERROR_ACCESS_DENIED?"AccessDenied":"TargetClosed");
    FILETIME created{},exit{},kernel{},user{};
    if(!GetProcessTimes(process.value,&created,&exit,&kernel,&user)) throw Error("TargetClosed");
    return (static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
std::vector<int> runtime_id(IUIAutomationElement* e) {
    SAFEARRAY* raw=nullptr;check(e->GetRuntimeId(&raw),"TargetClosed");
    const auto release=[](SAFEARRAY* p){if(p)SafeArrayDestroy(p);};
    std::unique_ptr<SAFEARRAY,decltype(release)> ids(raw,release);
    if(!raw)throw Error("TargetUnavailable");
    LONG low=0,high=-1;check(SafeArrayGetLBound(raw,1,&low));check(SafeArrayGetUBound(raw,1,&high));
    std::vector<int> out;
    for(LONG i=low;i<=high;++i){int value=0;check(SafeArrayGetElement(raw,&i,&value));out.push_back(value);}
    return out;
}
bool password(IUIAutomationElement* e) { BOOL value=TRUE;check(e->get_CurrentIsPassword(&value),"TargetClosed");return value!=FALSE; }
bool editable(IUIAutomationElement* e) {
    if(password(e))throw Error("ProtectedInput");
    BOOL enabled=FALSE,focusable=FALSE;check(e->get_CurrentIsEnabled(&enabled));check(e->get_CurrentIsKeyboardFocusable(&focusable));
    if(!enabled || !focusable)return false;
    if(auto value=pattern<IUIAutomationValuePattern>(e,UIA_ValuePatternId)){
        BOOL read_only=TRUE;if(SUCCEEDED(value->get_CurrentIsReadOnly(&read_only))&&!read_only)return true;
    }
    if(auto text=pattern<IUIAutomationTextPattern>(e,UIA_TextPatternId)){
        ComPtr<IUIAutomationTextRange> range;
        if(SUCCEEDED(text->get_DocumentRange(&range))){
            Variant attr;
            if(SUCCEEDED(range->GetAttributeValue(UIA_IsReadOnlyAttributeId,&attr.value))&&attr.value.vt==VT_BOOL&&attr.value.boolVal==VARIANT_FALSE)return true;
        }
    }
    // TextPattern is also exposed by read-only pages. Never infer editing from
    // the Document/Edit control type alone when the provider cannot prove it.
    return false;
}
TextContext text_context(IUIAutomationElement* e) {
    TextContext ctx;
    if(password(e))throw Error("ProtectedInput");
    if(auto text=pattern<IUIAutomationTextPattern>(e,UIA_TextPatternId)){
        ComPtr<IUIAutomationTextRange> doc;
        if(SUCCEEDED(text->get_DocumentRange(&doc))){
            BString full;if(SUCCEEDED(doc->GetText(65537,&full.value))&&full.str().size()<=65536){ctx.before=full.str();ctx.readable=true;}
            ComPtr<IUIAutomationTextRangeArray> selections;
            if(ctx.readable&&SUCCEEDED(text->GetSelection(&selections))){
                int count=0;selections->get_Length(&count);
                if(count==1){
                    ComPtr<IUIAutomationTextRange> selection,left,right;
                    if(SUCCEEDED(selections->GetElement(0,&selection))&&SUCCEEDED(doc->Clone(&left))&&SUCCEEDED(doc->Clone(&right))&&
                       SUCCEEDED(left->MoveEndpointByRange(TextPatternRangeEndpoint_End,selection.Get(),TextPatternRangeEndpoint_Start))&&
                       SUCCEEDED(right->MoveEndpointByRange(TextPatternRangeEndpoint_Start,selection.Get(),TextPatternRangeEndpoint_End))){
                        BString a,b;
                        if(SUCCEEDED(left->GetText(65537,&a.value))&&SUCCEEDED(right->GetText(65537,&b.value))){ctx.prefix=a.str();ctx.suffix=b.str();ctx.cursor_known=ctx.prefix.size()+ctx.suffix.size()<=ctx.before.size();}
                    }
                }
            }
        }
    }
    if(!ctx.readable)if(auto value=pattern<IUIAutomationValuePattern>(e,UIA_ValuePatternId)){
        BString b;if(SUCCEEDED(value->get_CurrentValue(&b.value))&&b.str().size()<=65536){ctx.before=b.str();ctx.readable=true;}
    }
    return ctx;
}
static bool word(wchar_t c) { WORD kind=0;return GetStringTypeW(CT_CTYPE1,&c,1,&kind)&&((kind&(C1_ALPHA|C1_DIGIT))!=0); }
std::wstring spaced(const std::wstring& text,const TextContext& ctx) {
    if(text.empty()||!ctx.cursor_known)return text;
    auto result=text;
    if(!ctx.prefix.empty()&&word(ctx.prefix.back())&&word(result.front()))result.insert(result.begin(),L' ');
    if(!ctx.suffix.empty()&&word(ctx.suffix.front())&&word(result.back()))result.push_back(L' ');
    return result;
}
}
