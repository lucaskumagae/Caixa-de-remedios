// Sensores de tampa, LEDs e buzzer. Tudo sem delay(), atualizado a cada loop.
#pragma once

#include <Arduino.h>

#include "config.h"

class Tampas {
 public:
  void iniciar();
  // Lê os sensores com debounce. Retorna a máscara dos compartimentos que
  // acabaram de ser abertos (bit 0 = compartimento 1).
  uint8_t atualizar(uint32_t agoraMs);
  bool aberta(uint8_t compartimento) const { return estavel_[compartimento]; }

 private:
  bool estavel_[caixa::NUM_COMPARTIMENTOS] = {};
  bool ultimaLeitura_[caixa::NUM_COMPARTIMENTOS] = {};
  uint32_t mudouEm_[caixa::NUM_COMPARTIMENTOS] = {};
};

enum class ModoLed : uint8_t { Apagado, Aceso, PiscaRapido, PiscaSinal };

class Leds {
 public:
  void iniciar();
  void definir(uint8_t compartimento, ModoLed modo) { modos_[compartimento] = modo; }
  void atualizar(uint32_t agoraMs);

 private:
  ModoLed modos_[caixa::NUM_COMPARTIMENTOS] = {};
};

class Buzzer {
 public:
  void iniciar();
  void definir(bool tocando);
  void atualizar(uint32_t agoraMs);

 private:
  bool tocando_ = false;
  bool somLigado_ = false;
};
