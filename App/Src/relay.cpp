#include "relay.hpp"
#include "main.h"
#include "stm32h7xx_hal_def.h"
#include <cstring>


void Relay::leftOff() {

    HAL_GPIO_WritePin(RELAY_L_GPIO_Port, RELAY_L_Pin, GPIO_PIN_RESET);
    leftStatus = RelayStatus::OFF;

}

void Relay::leftOn() {

    HAL_GPIO_WritePin(RELAY_L_GPIO_Port, RELAY_L_Pin, GPIO_PIN_SET);
    leftStatus = RelayStatus::ON;

}

void Relay::rightOff() {

    HAL_GPIO_WritePin(RELAY_R_GPIO_Port, RELAY_R_Pin, GPIO_PIN_RESET);
    rightStatus = RelayStatus::OFF;

}

void Relay::rightOn() {

    HAL_GPIO_WritePin(RELAY_R_GPIO_Port, RELAY_R_Pin, GPIO_PIN_SET);
    rightStatus = RelayStatus::ON;

}