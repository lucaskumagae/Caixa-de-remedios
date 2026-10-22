#include "motor_de_doses.h"

#include <algorithm>

namespace caixa {

bool ehAlerta(TipoEvento tipo) {
  return tipo == TipoEvento::Esquecida || tipo == TipoEvento::TampaErrada ||
         tipo == TipoEvento::AberturaForaDeHorario;
}

const char* nomeDoEvento(TipoEvento tipo) {
  switch (tipo) {
    case TipoEvento::AlarmeIniciado: return "alarme";
    case TipoEvento::TomadaNoHorario: return "tomada";
    case TipoEvento::TomadaComAtraso: return "atrasada";
    case TipoEvento::Esquecida: return "esquecida";
    case TipoEvento::TampaErrada: return "tampa_errada";
    case TipoEvento::AberturaForaDeHorario: return "fora_de_horario";
  }
  return "?";
}

MotorDeDoses::MotorDeDoses(Regras regras) : regras_(regras) {}

HoraLocal MotorDeDoses::horaAgendada(uint32_t dia, uint16_t horario) {
  return static_cast<HoraLocal>(dia) * 86400 + static_cast<HoraLocal>(horario) * 60;
}

void MotorDeDoses::definirAgenda(const std::vector<Medicamento>& agenda, HoraLocal ignorarAntesDe) {
  agenda_ = agenda;

  // Doses ativas de remédios removidos, desativados ou com horário alterado param de tocar.
  ativas_.erase(std::remove_if(ativas_.begin(), ativas_.end(),
                               [this](const DoseAtiva& d) {
                                 for (const auto& m : agenda_) {
                                   if (m.id != d.medId || !m.ativo) continue;
                                   bool mesmoHorario = std::find(m.horarios.begin(), m.horarios.end(),
                                                                 d.horario) != m.horarios.end();
                                   return !mesmoHorario;
                                 }
                                 return true;
                               }),
                ativas_.end());
  // Se o compartimento mudou, o LED passa a indicar o novo.
  for (auto& d : ativas_) {
    for (const auto& m : agenda_) {
      if (m.id == d.medId) d.compartimento = m.compartimento;
    }
  }

  if (ignorarAntesDe <= 0) return;
  uint32_t hoje = diaDe(ignorarAntesDe);
  for (const auto& m : agenda_) {
    for (uint16_t h : m.horarios) {
      for (uint32_t d = hoje - 1; d <= hoje; ++d) {
        if (horaAgendada(d, h) < ignorarAntesDe && !ativa(m.id, d, h)) {
          marcarResolvida(m.id, d, h);
        }
      }
    }
  }
}

void MotorDeDoses::marcarResolvida(uint16_t medId, uint32_t dia, uint16_t horario) {
  if (!resolvida(medId, dia, horario)) resolvidas_.push_back({medId, dia, horario});
}

bool MotorDeDoses::resolvida(uint16_t medId, uint32_t dia, uint16_t horario) const {
  return std::any_of(resolvidas_.begin(), resolvidas_.end(), [&](const Chave& c) {
    return c.medId == medId && c.dia == dia && c.horario == horario;
  });
}

bool MotorDeDoses::ativa(uint16_t medId, uint32_t dia, uint16_t horario) const {
  return std::any_of(ativas_.begin(), ativas_.end(), [&](const DoseAtiva& d) {
    return d.medId == medId && d.dia == dia && d.horario == horario;
  });
}

std::vector<Evento> MotorDeDoses::atualizar(HoraLocal agora) {
  std::vector<Evento> eventos;
  const uint32_t hoje = diaDe(agora);
  const HoraLocal tolerancia = static_cast<HoraLocal>(regras_.toleranciaMin) * 60;

  // Só é preciso lembrar de ontem e hoje (a tolerância pode atravessar a meia-noite).
  resolvidas_.erase(std::remove_if(resolvidas_.begin(), resolvidas_.end(),
                                   [hoje](const Chave& c) { return c.dia + 1 < hoje; }),
                    resolvidas_.end());

  for (auto it = ativas_.begin(); it != ativas_.end();) {
    if (agora >= horaAgendada(it->dia, it->horario) + tolerancia) {
      eventos.push_back({agora, TipoEvento::Esquecida, it->medId, it->compartimento, it->dia, it->horario});
      marcarResolvida(it->medId, it->dia, it->horario);
      it = ativas_.erase(it);
    } else {
      ++it;
    }
  }

  for (const auto& m : agenda_) {
    if (!m.ativo) continue;
    for (uint16_t h : m.horarios) {
      for (uint32_t d = hoje - 1; d <= hoje; ++d) {
        const HoraLocal inicio = horaAgendada(d, h);
        if (agora < inicio || agora >= inicio + tolerancia) continue;
        if (!(m.diasSemana & (1u << diaDaSemana(d)))) continue;
        if (resolvida(m.id, d, h) || ativa(m.id, d, h)) continue;
        ativas_.push_back({m.id, m.compartimento, d, h, false});
        eventos.push_back({agora, TipoEvento::AlarmeIniciado, m.id, m.compartimento, d, h});
      }
    }
  }
  return eventos;
}

std::vector<Evento> MotorDeDoses::tampaAberta(uint8_t compartimento, HoraLocal agora) {
  std::vector<Evento> eventos;
  const HoraLocal noHorario = static_cast<HoraLocal>(regras_.noHorarioMin) * 60;

  for (auto it = ativas_.begin(); it != ativas_.end();) {
    if (it->compartimento != compartimento) {
      ++it;
      continue;
    }
    TipoEvento tipo = agora - horaAgendada(it->dia, it->horario) <= noHorario ? TipoEvento::TomadaNoHorario
                                                                             : TipoEvento::TomadaComAtraso;
    eventos.push_back({agora, tipo, it->medId, compartimento, it->dia, it->horario});
    marcarResolvida(it->medId, it->dia, it->horario);
    it = ativas_.erase(it);
  }
  if (!eventos.empty()) return eventos;

  if (!ativas_.empty()) {
    // Regra: tampa errada não para o alarme; o LED certo passa a piscar rápido.
    for (auto& d : ativas_) d.tampaErrada = true;
    eventos.push_back({agora, TipoEvento::TampaErrada, 0, compartimento, diaDe(agora), minutoDe(agora)});
  } else {
    eventos.push_back({agora, TipoEvento::AberturaForaDeHorario, 0, compartimento, diaDe(agora), minutoDe(agora)});
  }
  return eventos;
}

EstadoLed MotorDeDoses::led(uint8_t compartimento) const {
  EstadoLed estado = EstadoLed::Apagado;
  for (const auto& d : ativas_) {
    if (d.compartimento != compartimento) continue;
    if (d.tampaErrada) return EstadoLed::PiscandoRapido;
    estado = EstadoLed::Aceso;
  }
  return estado;
}

}  // namespace caixa
