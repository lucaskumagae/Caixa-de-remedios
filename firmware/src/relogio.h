// Hora da caixa. Ordem de preferência: RTC DS3231 (funciona sem internet),
// NTP quando há Wi-Fi (e corrige o RTC), ou a hora enviada pelo celular no painel.
#pragma once

#include <time.h>

#include "agenda.h"

namespace relogio {

void iniciar();
// Chamar no loop: grava no RTC a hora recebida por NTP.
void atualizar();

bool horaValida();
bool temRtc();
caixa::HoraLocal agoraLocal();
// Acerta a hora a partir de um instante UTC (segundos desde 1970).
void acertar(time_t utc);

}  // namespace relogio
