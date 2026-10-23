// Configuração de hardware e regras. Ajuste os pinos conforme a montagem.
#pragma once

#include <Arduino.h>

#include "motor_de_doses.h"

// Um sensor de tampa e um LED por compartimento, na mesma ordem (compartimento 1..4).
// Sensor: chave ou reed ligado ao GND, com INPUT_PULLUP. HIGH = tampa aberta.
constexpr uint8_t PINOS_TAMPA[caixa::NUM_COMPARTIMENTOS] = {23, 32, 33, 25};
constexpr uint8_t PINOS_LED[caixa::NUM_COMPARTIMENTOS] = {26, 27, 14, 13};
constexpr uint8_t PINO_BUZZER = 19;  // buzzer passivo
// RTC DS3231 no I2C padrão do ESP32: SDA = 21, SCL = 22.

constexpr uint16_t FREQUENCIA_BUZZER_HZ = 1000;
constexpr uint32_t DEBOUNCE_MS = 50;

// Brasília, sem horário de verão desde 2019.
constexpr long FUSO_SEGUNDOS = -3 * 3600;

const caixa::Regras REGRAS = {
    /*noHorarioMin=*/15,
    /*toleranciaMin=*/60,
};

constexpr const char* NOME_MDNS = "caixa";  // painel em http://caixa.local
