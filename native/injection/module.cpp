#include "tracker.h"
#include "focused.h"
namespace flow {
struct ComApartment {ComApartment(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));}~ComApartment(){CoUninitialize();}};
struct Runtime {
    ComApartment com;
    Automation automation;
    Tracker tracker{automation};
    Clipboard clipboard;
    explicit Runtime(HWND owner):clipboard(owner){}
};
std::unique_ptr<Runtime> runtime;
std::string output;
}
extern "C" __declspec(dllexport) const char* flow_request(const char* input) noexcept {
    using namespace flow;
    try{
        const auto request=Json::parse(input);const auto op=request.at("op").get<std::string>();Json result;
        if(op=="init"){
            const auto owner=reinterpret_cast<HWND>(request.at("owner").get<std::uintptr_t>());
            DWORD pid=0;GetWindowThreadProcessId(owner,&pid);if(pid!=GetCurrentProcessId())throw Error("OwnWindow");
            if(!runtime)runtime=std::make_unique<Runtime>(owner);result=status("Ready");
        }else if(op=="shutdown"){runtime.reset();result=status("Stopped");}
        else{
            if(!runtime)throw Error("UiAutomationUnavailable");
            if(op=="state")result=runtime->tracker.state();
            else if(op=="capture")result=runtime->tracker.capture();
            else if(op=="pin")result=runtime->tracker.pin();
            else if(op=="unpin"){runtime->tracker.unpin();result=runtime->tracker.state();}
            else if(op=="arm"){runtime->tracker.arm();result=runtime->tracker.state();}
            else if(op=="cancel"){runtime->tracker.cancel();result=runtime->tracker.state();}
            else if(op=="current"){auto e=runtime->automation.focused();result=Json{{"status","Resolved"},{"target",runtime->automation.describe(e.Get())}};}
            else if(op=="resolve"){auto e=runtime->automation.resolve(request.at("target").get<TargetDescriptor>());result=Json{{"status","Resolved"},{"target",runtime->automation.describe(e.Get())}};}
            else if(op=="restore")result=runtime->clipboard.restore();
            else if(op=="direct"||op=="paste"||op=="clipboard"){
                const auto text=wide(request.at("text").get<std::string>());
                if(text.size()>50000||text.find(L'\0')!=std::wstring::npos)throw Error("InvalidText");
                if(text.empty()||std::all_of(text.begin(),text.end(),[](wchar_t c){return iswspace(c)!=0;}))result=status("Empty");
                else if(op=="clipboard")result=runtime->clipboard.put(text);
                else if(op=="direct")result=direct_insert(runtime->automation,request.at("target").get<TargetDescriptor>(),text);
                else result=paste_insert(runtime->automation,runtime->clipboard,request.at("target").get<TargetDescriptor>(),text);
            }else throw Error("InvalidRequest");
            result["clipboardRecoverable"]=runtime->clipboard.recoverable();
        }
        output=result.dump();
    }catch(const flow::Error& e){output=flow::Json{{"status","Failed"},{"code",e.code},{"hresult",static_cast<long>(e.hr)}}.dump();}
    catch(const std::exception&){output="{\"status\":\"Failed\",\"code\":\"BackendFailed\"}";}
    return flow::output.c_str();
}
