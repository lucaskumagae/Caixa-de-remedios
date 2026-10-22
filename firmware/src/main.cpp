// Caixa de remédios temporizada.
// A caixa funciona sozinha: agenda, hora e histórico ficam no ESP32. O Wi-Fi
// só serve para o painel de configuração e para acertar a hora por NTP.
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <vector>

#include "armazenamento.h"
#include "config.h"
#include "hardware.h"
#include "motor_de_doses.h"
#include "painel.h"
#include "relogio.h"
#include "segredos_ou_exemplo.h"

namespace {

std::vector<caixa::Medicamento> remedios;
caixa::MotorDeDoses motor(REGRAS);
Tampas tampas;
Leds leds;
Buzzer buzzer;

bool historicoCarregado = false;
uint32_t ultimaAtualizacaoMs = 0;

std::string nomeDoRemedio(uint16_t id) {
  for (const auto& m : remedios) {
    if (m.id == id) return m.nome;
  }
  return "";
}

void registrar(const std::vector<caixa::Evento>& eventos) {
  for (const auto& e : eventos) {
    std::string nome = nomeDoRemedio(e.medId);
    Serial.printf("[evento] %s compartimento=%u %s\n", caixa::nomeDoEvento(e.tipo), e.compartimento + 1,
                  nome.c_str());
    armazenamento::registrar(e, nome);
  }
}

void iniciarWifi() {
  // A rede própria da caixa fica sempre disponível, para configurar sem roteador.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_NOME, AP_SENHA);
  Serial.printf("[wifi] rede da caixa: %s (http://%s)\n", AP_NOME, WiFi.softAPIP().toString().c_str());

  // Sem esperar: se a rede da casa não responder, a caixa segue funcionando.
  if (strlen(WIFI_SSID) > 0) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
  }
  if (MDNS.begin(NOME_MDNS)) MDNS.addService("http", "tcp", 80);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  tampas.iniciar();
  leds.iniciar();
  buzzer.iniciar();

  armazenamento::iniciar();
  remedios = armazenamento::carregarRemedios();
  Serial.printf("[boot] %u remedio(s) carregado(s)\n", static_cast<unsigned>(remedios.size()));
  // 0: no boot, uma dose cujo horário passou durante a queda de energia ainda toca.
  motor.definirAgenda(remedios, 0);

  relogio::iniciar();
  iniciarWifi();
  painel::iniciar(remedios, motor);
}

void loop() {
  const uint32_t agoraMs = millis();
  painel::atualizar();
  relogio::atualizar();

  const bool horaValida = relogio::horaValida();
  if (horaValida && !historicoCarregado) {
    armazenamento::carregarResolvidas(motor, caixa::diaDe(relogio::agoraLocal()));
    historicoCarregado = true;
  }

  uint8_t abertas = tampas.atualizar(agoraMs);
  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    if (!(abertas & (1u << c))) continue;
    if (horaValida) {
      registrar(motor.tampaAberta(c, relogio::agoraLocal()));
    } else {
      Serial.printf("[tampa] compartimento %u aberto (sem hora certa, nao registrado)\n", c + 1);
    }
  }

  if (horaValida && historicoCarregado && agoraMs - ultimaAtualizacaoMs >= 1000) {
    ultimaAtualizacaoMs = agoraMs;
    registrar(motor.atualizar(relogio::agoraLocal()));
  }

  for (uint8_t c = 0; c < caixa::NUM_COMPARTIMENTOS; ++c) {
    ModoLed modo = ModoLed::PiscaSinal;  // sem hora certa a caixa não sabe quando tocar
    if (horaValida) {
      switch (motor.led(c)) {
        case caixa::EstadoLed::Apagado: modo = ModoLed::Apagado; break;
        case caixa::EstadoLed::Aceso: modo = ModoLed::Aceso; break;
        case caixa::EstadoLed::PiscandoRapido: modo = ModoLed::PiscaRapido; break;
      }
    }
    leds.definir(c, modo);
  }
  leds.atualizar(agoraMs);

  buzzer.definir(motor.buzzerLigado());
  buzzer.atualizar(agoraMs);

  delay(5);
}
