#include "relogio.h"

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>

#include "config.h"

namespace relogio {

namespace {
RTC_DS3231 rtc;
bool rtcPresente = false;
volatile bool ntpSincronizou = false;

// Qualquer hora antes de 2024 significa que o ESP ainda não sabe a hora.
constexpr time_t HORA_MINIMA = 1704067200;

void definirHoraDoSistema(time_t utc) {
  timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
}

void aoSincronizarNtp(timeval*) { ntpSincronizou = true; }
}  // namespace

void iniciar() {
  Wire.begin();
  rtcPresente = rtc.begin();
  if (rtcPresente && !rtc.lostPower()) {
    // O RTC guarda UTC.
    definirHoraDoSistema(rtc.now().unixtime());
    Serial.println("[relogio] hora carregada do RTC");
  } else if (rtcPresente) {
    Serial.println("[relogio] RTC perdeu a hora (bateria?); aguardando NTP ou o painel");
  } else {
    Serial.println("[relogio] RTC nao encontrado; a hora depende de NTP ou do painel");
  }

  sntp_set_time_sync_notification_cb(aoSincronizarNtp);
  configTime(0, 0, "a.st1.ntp.br", "pool.ntp.org");
}

void atualizar() {
  if (!ntpSincronizou) return;
  ntpSincronizou = false;
  if (rtcPresente) rtc.adjust(DateTime(static_cast<uint32_t>(time(nullptr))));
  Serial.println("[relogio] hora sincronizada por NTP");
}

bool horaValida() { return time(nullptr) > HORA_MINIMA; }

bool temRtc() { return rtcPresente; }

caixa::HoraLocal agoraLocal() { return static_cast<caixa::HoraLocal>(time(nullptr)) + FUSO_SEGUNDOS; }

void acertar(time_t utc) {
  definirHoraDoSistema(utc);
  if (rtcPresente) rtc.adjust(DateTime(static_cast<uint32_t>(utc)));
}

}  // namespace relogio
