#include "tracker.h"
namespace flow {
HRESULT FocusHandler::HandleFocusChangedEvent(IUIAutomationElement* sender){
    try{
        auto candidate=automation_->candidate(sender);
        auto descriptor=automation_->describe(candidate.Get());
        // Ignore delayed events that no longer describe actual user focus.
        if(!automation_->matches_focus(candidate.Get(),descriptor))return S_OK;
        std::lock_guard lock(state_->mutex);state_->last=descriptor;state_->protected_focus=false;
        if(std::chrono::steady_clock::now()<state_->armed_until){state_->pinned=descriptor;state_->armed_until={};}
    }catch(const Error& e){if(e.code=="ProtectedInput"){std::lock_guard lock(state_->mutex);state_->protected_focus=true;state_->last.reset();}}
    catch(...){return E_FAIL;}
    return S_OK;
}
Tracker::Tracker(Automation& automation):automation_(automation){handler_=Microsoft::WRL::Make<FocusHandler>(&automation_,state_);check(automation_.api()->AddFocusChangedEventHandler(nullptr,handler_.Get()));}
Tracker::~Tracker(){automation_.api()->RemoveFocusChangedEventHandler(handler_.Get());}
Json Tracker::state(){std::lock_guard lock(state_->mutex);return Json{{"last",state_->last?Json(*state_->last):Json(nullptr)},{"pinned",state_->pinned?Json(*state_->pinned):Json(nullptr)},{"protected",state_->protected_focus},{"armed",std::chrono::steady_clock::now()<state_->armed_until}};}
Json Tracker::capture(){
    try{auto element=automation_.focused();auto d=automation_.describe(element.Get());std::lock_guard lock(state_->mutex);state_->last=d;state_->protected_focus=false;}
    catch(const Error& e){if(e.code=="ProtectedInput"){std::lock_guard lock(state_->mutex);state_->protected_focus=true;state_->last.reset();}else if(e.code!="OwnWindow"&&e.code!="TargetNotEditable"&&e.code!="NoTarget")throw;}
    return state();
}
Json Tracker::pin(){auto e=automation_.focused();auto d=automation_.describe(e.Get());{std::lock_guard lock(state_->mutex);state_->last=d;state_->pinned=d;state_->protected_focus=false;state_->armed_until={};}return state();}
void Tracker::unpin(){std::lock_guard lock(state_->mutex);state_->pinned.reset();state_->armed_until={};}
void Tracker::arm(){std::lock_guard lock(state_->mutex);state_->armed_until=std::chrono::steady_clock::now()+std::chrono::seconds(15);}
void Tracker::cancel(){std::lock_guard lock(state_->mutex);state_->armed_until={};}
}
