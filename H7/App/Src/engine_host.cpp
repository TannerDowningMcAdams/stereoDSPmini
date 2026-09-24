#include "engine_host.hpp"
#include "engine_registry.hpp"

void EngineHost::init(const Config& config)
{
    config_ = config;
    arenas_.init();

    activeIndex_ = 0u;
    active_      = EngineRegistry::at(activeIndex_);
    active_->activate(arenas_, config_.sampleRate);
    takeDefaults();
    config_.processor->install(active_, appliedControls());

    // Taken at the G0's first valid frame, so the first echo already holds the defaults.
    (void) config_.link->setApplied(appliedParam_, appliedDiscrete_, defaultsSeq_);
    publish(H7_FLAG_ENGINE_READY);
    state_ = State::Running;
}

void EngineHost::poll()
{
    G0Spi::Request request;
    const bool haveRequest = config_.link->request(request);

    switch (state_)
    {
        case State::Running:
            if (haveRequest) { handleRequest(request); }
            break;

        case State::Parking:
            if (config_.processor->parked()) { swap(haveRequest ? &request : nullptr); }
            break;

        case State::Publishing:
            // Retried until the link takes it: the previous values wait for a frame.
            if (config_.link->setApplied(appliedParam_, appliedDiscrete_, defaultsSeq_))
            {
                publish(H7_FLAG_ENGINE_READY);
                state_ = State::Running;
            }
            break;
    }
}

void EngineHost::handleRequest(const G0Spi::Request& request)
{
    // With no request outstanding, defaultsSeq is what the G0 has adopted. Taking it
    // here means a restarted H7 does not mistake it for a new request.
    if (!request.defaultsPending) { defaultsSeq_ = request.defaultsSeq; }

    const uint8_t index = (request.engineId == ENGINE_ID_NONE) ? activeIndex_
                                                               : EngineRegistry::indexOf(request.engineId);
    const bool unknown = index == EngineRegistry::kNotFound;
    if (unknown != unknown_)
    {
        unknown_ = unknown;
        publish(H7_FLAG_ENGINE_READY);
    }
    if (unknown) { return; }

    if (index != activeIndex_)
    {
        targetIndex_ = index;
        // Not ready: from here the link stops applying params to either engine.
        publish(H7_FLAG_LOADING);
        config_.processor->park();
        state_ = State::Parking;
        return;
    }

    // Defaults for the engine already running reach it through the link, like any
    // other control set, once the link takes them.
    if (request.engineId != ENGINE_ID_NONE && request.defaultsPending && request.defaultsSeq != defaultsSeq_)
    {
        takeDefaults();
        defaultsSeq_ = request.defaultsSeq;
        state_ = State::Publishing;
    }
}

// The engine is parked: PendSV no longer calls it.
void EngineHost::swap(const G0Spi::Request* request)
{
    active_->deactivate();
    arenas_.reset();
    activeIndex_ = targetIndex_;
    active_      = EngineRegistry::at(activeIndex_);
    active_->activate(arenas_, config_.sampleRate);

    // The G0 may have moved on while the old engine faded; its latest frame decides.
    // Values it sent for another engine, or before a defaults request, are not used.
    const bool wanted = request != nullptr && request->engineId == active_->id();
    if (wanted && !request->defaultsPending)
    {
        for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { appliedParam_[i] = request->param[i]; }
        appliedDiscrete_ = request->discrete;
    }
    else
    {
        takeDefaults();
        if (wanted) { defaultsSeq_ = request->defaultsSeq; }
    }

    config_.processor->install(active_, appliedControls());
    config_.processor->resume();
    publish(H7_FLAG_LOADING);
    state_ = State::Publishing;
}

void EngineHost::takeDefaults()
{
    const EngineInfo& info = active_->info();
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { appliedParam_[i] = info.paramDefault[i]; }
    appliedDiscrete_ = info.discreteDefault();
}

EngineControls EngineHost::appliedControls() const
{
    EngineControls controls {};
    for (uint32_t i = 0; i < SPI_PARAM_COUNT; i++) { controls.params[i] = appliedParam_[i] * (1.0f / 65535.0f); }
    controls.discrete = appliedDiscrete_;
    return controls;
}

void EngineHost::publish(uint16_t flags)
{
    G0Spi::EngineStatus status {};
    status.descriptor  = active_->info().descriptor();
    status.activeId    = active_->id();
    status.activeIndex = activeIndex_;
    status.flags       = static_cast<uint16_t>(flags | (unknown_ ? H7_FLAG_ENGINE_UNKNOWN : 0u));
    config_.link->publishEngine(status);
}
