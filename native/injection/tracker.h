#pragma once
#include "target.h"
namespace flow {
struct TrackerState {
    std::mutex mutex;
    std::optional<TargetDescriptor> last,pinned;
    bool protected_focus=false;
    std::chrono::steady_clock::time_point armed_until{};
};
class FocusHandler final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,IUIAutomationFocusChangedEventHandler> {
    Automation* automation_;
    std::shared_ptr<TrackerState> state_;
public:
    FocusHandler(Automation* automation,std::shared_ptr<TrackerState> state):automation_(automation),state_(std::move(state)){}
    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) override;
};
class Tracker {
    Automation& automation_;
    std::shared_ptr<TrackerState> state_=std::make_shared<TrackerState>();
    ComPtr<FocusHandler> handler_;
public:
    explicit Tracker(Automation& automation);
    ~Tracker();
    Json state();
    Json capture();
    Json pin();
    void unpin();
    void arm();
    void cancel();
};
}
