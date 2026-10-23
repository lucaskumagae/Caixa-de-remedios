// Regras de negócio da caixa: quando uma dose começa a tocar, quando conta
// como tomada, atrasada ou esquecida, e o que cada LED e o buzzer devem fazer.
// Não acessa hardware: recebe a hora e as aberturas de tampa e devolve eventos.
#pragma once

#include <cstdint>
#include <vector>

#include "agenda.h"

namespace caixa {

enum class TipoEvento : uint8_t {
  AlarmeIniciado,
  TomadaNoHorario,
  TomadaComAtraso,
  Esquecida,
  TampaErrada,
  AberturaForaDeHorario,
};

// Eventos que aparecem em vermelho no histórico.
bool ehAlerta(TipoEvento tipo);
const char* nomeDoEvento(TipoEvento tipo);

struct Evento {
  HoraLocal quando = 0;
  TipoEvento tipo = TipoEvento::AlarmeIniciado;
  uint16_t medId = 0;         // 0 quando o evento não é de um remédio específico
  uint8_t compartimento = 0;
  uint32_t dia = 0;           // dia agendado da dose (diaDe)
  uint16_t horario = 0;       // horário agendado da dose, em minutos
};

enum class EstadoLed : uint8_t { Apagado, Aceso, PiscandoRapido };

struct Regras {
  // Aberta até esse tempo depois do horário: "tomada no horário".
  uint16_t noHorarioMin = 15;
  // Depois disso o alarme desiste e a dose vira "esquecida".
  uint16_t toleranciaMin = 60;
};

struct DoseAtiva {
  uint16_t medId;
  uint8_t compartimento;
  uint32_t dia;
  uint16_t horario;
  bool tampaErrada;  // alguém abriu outro compartimento durante este alarme
};

class MotorDeDoses {
 public:
  explicit MotorDeDoses(Regras regras = Regras());

  // Troca a agenda. Doses cujo horário já passou antes de `ignorarAntesDe` não
  // disparam (evita tocar na hora em que alguém cadastra um remédio das 08:00
  // às 08:10). No boot use 0, para recuperar uma dose perdida por queda de energia.
  void definirAgenda(const std::vector<Medicamento>& agenda, HoraLocal ignorarAntesDe);

  // Informa uma dose já resolvida (lida do histórico no boot), para não tocar de novo.
  void marcarResolvida(uint16_t medId, uint32_t dia, uint16_t horario);

  std::vector<Evento> atualizar(HoraLocal agora);
  std::vector<Evento> tampaAberta(uint8_t compartimento, HoraLocal agora);

  EstadoLed led(uint8_t compartimento) const;
  bool buzzerLigado() const { return !ativas_.empty(); }
  const std::vector<DoseAtiva>& dosesAtivas() const { return ativas_; }

 private:
  struct Chave {
    uint16_t medId;
    uint32_t dia;
    uint16_t horario;
  };

  bool resolvida(uint16_t medId, uint32_t dia, uint16_t horario) const;
  bool ativa(uint16_t medId, uint32_t dia, uint16_t horario) const;
  static HoraLocal horaAgendada(uint32_t dia, uint16_t horario);

  Regras regras_;
  std::vector<Medicamento> agenda_;
  std::vector<DoseAtiva> ativas_;
  std::vector<Chave> resolvidas_;
};

}  // namespace caixa
