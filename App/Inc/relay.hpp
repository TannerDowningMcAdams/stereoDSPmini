#pragma once

#include "pin.hpp"
#include <cstdint>

class Relay {
public:

    struct Config
    {
        Pin left;
        Pin right;
    };

    Relay() = default;
    ~Relay() = default;

    enum class RelayStatus { OFF, ON };

    void init(const Config& config) { config_ = config; }

    void leftOff();
    void leftOn();
    void rightOff();
    void rightOn();
    
    RelayStatus getLeftStatus(){ return leftStatus_; }
    RelayStatus getRightStatus(){ return rightStatus_; }
    
private:

    Config config_ {};
    RelayStatus leftStatus_ = RelayStatus::OFF;
    RelayStatus rightStatus_ = RelayStatus::OFF;

} ;
