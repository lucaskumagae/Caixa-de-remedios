// Tipos da agenda de remédios. Não depende do Arduino, para poder ser
// testado no computador (pio test -e native).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace caixa {

constexpr uint8_t NUM_COMPARTIMENTOS = 4;
constexpr uint8_t TODOS_OS_DIAS = 0x7F;  // bit 0 = domingo ... bit 6 = sábado

struct Medicamento {
  uint16_t id = 0;
  std::string nome;
  std::string dose;                // texto livre, ex.: "1 comprimido de 50mg"
  uint8_t compartimento = 0;       // 0..NUM_COMPARTIMENTOS-1
  std::vector<uint16_t> horarios;  // minutos desde a meia-noite
  uint8_t diasSemana = TODOS_OS_DIAS;
  bool ativo = true;
};

// Instante em hora local, já com o fuso aplicado (segundos desde 1970-01-01
// 00:00 no horário local). Mantido como inteiro para os testes ficarem simples.
using HoraLocal = int64_t;

inline uint32_t diaDe(HoraLocal t) { return static_cast<uint32_t>(t / 86400); }
inline uint16_t minutoDe(HoraLocal t) { return static_cast<uint16_t>((t % 86400) / 60); }
// 1970-01-01 foi uma quinta-feira; 0 = domingo.
inline uint8_t diaDaSemana(uint32_t dia) { return static_cast<uint8_t>((dia + 4) % 7); }

// "08:30" -> 510. Retorna -1 se o texto não for um horário válido.
int horarioDeTexto(const std::string& texto);
// 510 -> "08:30"
std::string horarioParaTexto(uint16_t minutos);

// Retorna uma mensagem de erro em português, ou string vazia se estiver válido.
std::string validar(const Medicamento& m);

}  // namespace caixa
