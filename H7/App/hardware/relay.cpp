#include "relay.hpp"

void Relay::leftOff()
{
    HAL_GPIO_WritePin(config_.left.port, config_.left.mask, GPIO_PIN_RESET);
    leftStatus_ = RelayStatus::OFF;
}

void Relay::leftOn()
{
    HAL_GPIO_WritePin(config_.left.port, config_.left.mask, GPIO_PIN_SET);
    leftStatus_ = RelayStatus::ON;
}

void Relay::rightOff()
{
    HAL_GPIO_WritePin(config_.right.port, config_.right.mask, GPIO_PIN_RESET);
    rightStatus_ = RelayStatus::OFF;
}

void Relay::rightOn()
{
    HAL_GPIO_WritePin(config_.right.port, config_.right.mask, GPIO_PIN_SET);
    rightStatus_ = RelayStatus::ON;
}
