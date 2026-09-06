#include "clipboard.h"
namespace flow {
namespace {
class Lock {
public:
    explicit Lock(HWND owner){for(int n=0;n<6;++n){if(OpenClipboard(owner))return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw Error("ClipboardBusy");}
    ~Lock(){CloseClipboard();}
    Lock(const Lock&)=delete;
    Lock& operator=(const Lock&)=delete;
};
struct Global {
    HGLOBAL value;
    explicit Global(size_t size):value(GlobalAlloc(GMEM_MOVEABLE,size)){if(!value)throw Error("ClipboardFailed");}
    ~Global(){if(value)GlobalFree(value);}
    Global(Global&& other) noexcept:value(std::exchange(other.value,nullptr)){}
    Global(const Global&)=delete;
    Global& operator=(const Global&)=delete;
};
Global allocate(const void* data,size_t size){Global memory(size);void* ptr=GlobalLock(memory.value);if(!ptr)throw Error("ClipboardFailed");memcpy(ptr,data,size);GlobalUnlock(memory.value);return memory;}
bool opaque(UINT id){return id==CF_BITMAP||id==CF_PALETTE||id==CF_METAFILEPICT||id==CF_ENHMETAFILE||id==CF_OWNERDISPLAY||id==CF_DSPBITMAP||id==CF_DSPMETAFILEPICT||id==CF_DSPENHMETAFILE||(id>=CF_PRIVATEFIRST&&id<=CF_PRIVATELAST)||(id>=CF_GDIOBJFIRST&&id<=CF_GDIOBJLAST);}
std::vector<ClipboardFormat> snapshot(){
    std::vector<ClipboardFormat> result;UINT id=0;size_t total=0;
    SetLastError(ERROR_SUCCESS);
    while((id=EnumClipboardFormats(id))!=0){
        if(opaque(id))throw Error("ClipboardPreserved");
        const auto memory=GetClipboardData(id);const auto size=memory?GlobalSize(memory):0;
        if(!size||size>32*1024*1024||total+size>32*1024*1024)throw Error("ClipboardPreserved");
        const auto ptr=GlobalLock(memory);if(!ptr)throw Error("ClipboardPreserved");
        ClipboardFormat format{id,std::vector<std::byte>(size)};memcpy(format.bytes.data(),ptr,size);GlobalUnlock(memory);
        total+=size;result.push_back(std::move(format));SetLastError(ERROR_SUCCESS);
    }
    if(GetLastError()!=ERROR_SUCCESS)throw Error("ClipboardPreserved");
    return result;
}
void replace(const std::vector<ClipboardFormat>& formats){
    // Allocate every format before changing ownership, so allocation failure
    // never destroys an existing clipboard.
    std::vector<Global> handles;handles.reserve(formats.size());
    for(const auto& f:formats)handles.push_back(allocate(f.bytes.data(),f.bytes.size()));
    if(!EmptyClipboard())throw Error("ClipboardFailed");
    for(size_t i=0;i<formats.size();++i){if(!SetClipboardData(formats[i].id,handles[i].value))throw Error("ClipboardFailed");handles[i].value=nullptr;}
}
}
bool Clipboard::recoverable(){if(backup_&&GetClipboardSequenceNumber()!=sequence_)backup_.reset();return backup_.has_value();}
Json Clipboard::put(const std::wstring& text){
    Lock lock(owner_);
    if(!backup_||GetClipboardSequenceNumber()!=sequence_)backup_=snapshot();
    std::vector<ClipboardFormat> replacement;
    const auto bytes=(text.size()+1)*sizeof(wchar_t);
    replacement.push_back({CF_UNICODETEXT,std::vector<std::byte>(bytes)});
    memcpy(replacement[0].bytes.data(),text.c_str(),bytes);
    // Ask Windows not to place private dictation into clipboard history/sync.
    const DWORD no=0;
    for(const auto name:{L"CanIncludeInClipboardHistory",L"CanUploadToCloudClipboard"}){
        const auto id=RegisterClipboardFormatW(name);if(id){replacement.push_back({id,std::vector<std::byte>(sizeof(no))});memcpy(replacement.back().bytes.data(),&no,sizeof(no));}
    }
    try{replace(replacement);}catch(...){try{replace(*backup_);backup_.reset();}catch(...){sequence_=GetClipboardSequenceNumber();}throw;}
    sequence_=GetClipboardSequenceNumber();return status("Copied");
}
Json Clipboard::restore(){
    if(!backup_)return status("NothingToRestore");
    Lock lock(owner_);
    if(GetClipboardSequenceNumber()!=sequence_){backup_.reset();return status("ClipboardChanged");}
    replace(*backup_);backup_.reset();sequence_=0;return status("Restored");
}
}
