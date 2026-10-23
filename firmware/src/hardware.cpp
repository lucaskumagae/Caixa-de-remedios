#include "hardware.h"

void Tampas::iniciar() {
  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    pinMode(PINOS_TAMPA[c], INPUT_PULLUP);
    estavel_[c] = ultimaLeitura_[c] = digitalRead(PINOS_TAMPA[c]) == HIGH;
  }
}

uint8_t Tampas::atualizar(uint32_t agoraMs) {
  uint8_t abertas = 0;
  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    bool leitura = digitalRead(PINOS_TAMPA[c]) == HIGH;
    if (leitura != ultimaLeitura_[c]) {
      ultimaLeitura_[c] = leitura;
      mudouEm_[c] = agoraMs;
    } else if (leitura != estavel_[c] && agoraMs - mudouEm_[c] >= DEBOUNCE_MS) {
      estavel_[c] = leitura;
      if (leitura) abertas |= 1u << c;
    }
  }
  return abertas;
}

void Leds::iniciar() {
  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    pinMode(PINOS_LED[c], OUTPUT);
    digitalWrite(PINOS_LED[c], LOW);
  }
}

void Leds::atualizar(uint32_t agoraMs) {
  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    bool ligado = false;
    switch (modos_[c]) {
      case ModoLed::Apagado: ligado = false; break;
      case ModoLed::Aceso: ligado = true; break;
      case ModoLed::PiscaRapido: ligado = (agoraMs / 150) % 2 == 0; break;
      // Piscada curta a cada 2 s: caixa sem hora certa.
      case ModoLed::PiscaSinal: ligado = agoraMs % 2000 < 100; break;
    }
    digitalWrite(PINOS_LED[c], ligado ? HIGH : LOW);
  }
}

void Buzzer::iniciar() {
  pinMode(PINO_BUZZER, OUTPUT);
  noTone(PINO_BUZZER);
}

void Buzzer::definir(bool tocando) { tocando_ = tocando; }

void Buzzer::atualizar(uint32_t agoraMs) {
  // Bipes de 400 ms, mais fáceis de perceber do que um som contínuo.
  bool deveSoar = tocando_ && (agoraMs / 400) % 2 == 0;
  if (deveSoar == somLigado_) return;
  somLigado_ = deveSoar;
  if (deveSoar) {
    tone(PINO_BUZZER, FREQUENCIA_BUZZER_HZ);
  } else {
    noTone(PINO_BUZZER);
  }
}
