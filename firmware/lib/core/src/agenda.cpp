#include "agenda.h"

#include <cstdio>

namespace caixa {

namespace {
constexpr size_t MAX_NOME = 40;
constexpr size_t MAX_DOSE = 40;
constexpr size_t MAX_HORARIOS = 8;
}  // namespace

int horarioDeTexto(const std::string& texto) {
  if (texto.size() != 5 || texto[2] != ':') return -1;
  for (size_t i : {0, 1, 3, 4}) {
    if (texto[i] < '0' || texto[i] > '9') return -1;
  }
  int hh = (texto[0] - '0') * 10 + (texto[1] - '0');
  int mm = (texto[3] - '0') * 10 + (texto[4] - '0');
  if (hh > 23 || mm > 59) return -1;
  return hh * 60 + mm;
}

std::string horarioParaTexto(uint16_t minutos) {
  char buf[6];
  std::snprintf(buf, sizeof(buf), "%02u:%02u", (minutos / 60) % 24, minutos % 60);
  return buf;
}

std::string validar(const Medicamento& m) {
  if (m.nome.empty()) return "Informe o nome do remédio.";
  if (m.nome.size() > MAX_NOME) return "O nome do remédio pode ter no máximo 40 caracteres.";
  if (m.dose.size() > MAX_DOSE) return "A dose pode ter no máximo 40 caracteres.";
  if (m.compartimento >= NUM_COMPARTIMENTOS) return "Compartimento inválido.";
  if (m.horarios.empty()) return "Informe pelo menos um horário.";
  if (m.horarios.size() > MAX_HORARIOS) return "Use no máximo 8 horários por remédio.";
  for (size_t i = 0; i < m.horarios.size(); ++i) {
    if (m.horarios[i] >= 24 * 60) return "Horário inválido.";
    for (size_t j = i + 1; j < m.horarios.size(); ++j) {
      if (m.horarios[i] == m.horarios[j]) return "Há horários repetidos.";
    }
  }
  if ((m.diasSemana & TODOS_OS_DIAS) == 0) return "Escolha pelo menos um dia da semana.";
  return "";
}

}  // namespace caixa
