#include "armazenamento.h"

#include <LittleFS.h>

#include <deque>

namespace armazenamento {

namespace {
constexpr const char* ARQ_REMEDIOS = "/remedios.json";
constexpr const char* ARQ_REMEDIOS_TMP = "/remedios.tmp";
constexpr const char* ARQ_HISTORICO = "/historico.jsonl";
constexpr const char* ARQ_HISTORICO_ANTIGO = "/historico.old";
constexpr size_t LIMITE_HISTORICO_BYTES = 48 * 1024;

// Chama `fn` para cada linha do histórico, do evento mais antigo para o mais novo.
template <typename Fn>
void paraCadaLinha(Fn fn) {
  for (const char* caminho : {ARQ_HISTORICO_ANTIGO, ARQ_HISTORICO}) {
    File f = LittleFS.open(caminho, "r");
    if (!f) continue;
    while (f.available()) {
      String linha = f.readStringUntil('\n');
      if (linha.length() > 0) fn(linha);
    }
    f.close();
  }
}
}  // namespace

bool iniciar() {
  if (!LittleFS.begin(true)) {
    Serial.println("[armazenamento] falha ao montar LittleFS");
    return false;
  }
  return true;
}

void remedioParaJson(const caixa::Medicamento& m, JsonObject saida) {
  saida["id"] = m.id;
  saida["nome"] = m.nome;
  saida["dose"] = m.dose;
  saida["compartimento"] = m.compartimento + 1;
  JsonArray horarios = saida["horarios"].to<JsonArray>();
  for (uint16_t h : m.horarios) horarios.add(caixa::horarioParaTexto(h));
  saida["diasSemana"] = m.diasSemana;
  saida["ativo"] = m.ativo;
}

std::string remedioDeJson(JsonObjectConst entrada, caixa::Medicamento& m) {
  m.nome = entrada["nome"] | "";
  m.dose = entrada["dose"] | "";
  int compartimento = entrada["compartimento"] | 0;
  if (compartimento < 1 || compartimento > caixa::NUM_COMPARTIMENTOS) return "Compartimento inválido.";
  m.compartimento = static_cast<uint8_t>(compartimento - 1);
  m.horarios.clear();
  for (JsonVariantConst v : entrada["horarios"].as<JsonArrayConst>()) {
    int h = caixa::horarioDeTexto(v | "");
    if (h < 0) return "Horário inválido. Use o formato HH:MM.";
    m.horarios.push_back(static_cast<uint16_t>(h));
  }
  m.diasSemana = entrada["diasSemana"] | caixa::TODOS_OS_DIAS;
  m.ativo = entrada["ativo"] | true;
  return caixa::validar(m);
}

namespace {
bool lerJson(const char* caminho, JsonDocument& doc) {
  File f = LittleFS.open(caminho, "r");
  if (!f) return false;
  DeserializationError erro = deserializeJson(doc, f);
  f.close();
  if (erro) Serial.printf("[armazenamento] %s ilegivel: %s\n", caminho, erro.c_str());
  return !erro;
}
}  // namespace

std::vector<caixa::Medicamento> carregarRemedios() {
  std::vector<caixa::Medicamento> remedios;
  JsonDocument doc;
  // Se a energia caiu entre gravar o temporário e renomear, o temporário completo é
  // o mais novo; se caiu durante a gravação, ele está pela metade e vale o anterior.
  if (!lerJson(ARQ_REMEDIOS_TMP, doc) && !lerJson(ARQ_REMEDIOS, doc)) return remedios;

  for (JsonObjectConst obj : doc.as<JsonArrayConst>()) {
    caixa::Medicamento m;
    m.id = obj["id"] | 0;
    if (m.id == 0 || !remedioDeJson(obj, m).empty()) continue;
    remedios.push_back(m);
  }
  return remedios;
}

bool salvarRemedios(const std::vector<caixa::Medicamento>& remedios) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& m : remedios) remedioParaJson(m, arr.add<JsonObject>());

  File f = LittleFS.open(ARQ_REMEDIOS_TMP, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (!ok) return false;

  // Grava num temporário e troca, para nunca ficar com um arquivo pela metade.
  LittleFS.remove(ARQ_REMEDIOS);
  return LittleFS.rename(ARQ_REMEDIOS_TMP, ARQ_REMEDIOS);
}

void registrar(const caixa::Evento& evento, const std::string& nomeRemedio) {
  File atual = LittleFS.open(ARQ_HISTORICO, "r");
  if (atual && atual.size() > LIMITE_HISTORICO_BYTES) {
    atual.close();
    LittleFS.remove(ARQ_HISTORICO_ANTIGO);
    LittleFS.rename(ARQ_HISTORICO, ARQ_HISTORICO_ANTIGO);
  } else if (atual) {
    atual.close();
  }

  JsonDocument doc;
  doc["t"] = evento.quando;
  doc["e"] = caixa::nomeDoEvento(evento.tipo);
  doc["c"] = evento.compartimento + 1;
  if (evento.medId != 0) {
    doc["m"] = evento.medId;
    doc["n"] = nomeRemedio;
    doc["d"] = evento.dia;
    doc["h"] = evento.horario;
  }
  if (caixa::ehAlerta(evento.tipo)) doc["alerta"] = true;

  File f = LittleFS.open(ARQ_HISTORICO, "a");
  if (!f) return;
  serializeJson(doc, f);
  f.print('\n');
  f.close();
}

void historicoParaJson(JsonArray saida, size_t limite) {
  std::deque<String> ultimas;
  paraCadaLinha([&](const String& linha) {
    ultimas.push_back(linha);
    if (ultimas.size() > limite) ultimas.pop_front();
  });
  for (auto it = ultimas.rbegin(); it != ultimas.rend(); ++it) {
    JsonDocument evento;
    if (deserializeJson(evento, *it) == DeserializationError::Ok) saida.add(evento);
  }
}

void carregarResolvidas(caixa::MotorDeDoses& motor, uint32_t hoje) {
  paraCadaLinha([&](const String& linha) {
    JsonDocument evento;
    if (deserializeJson(evento, linha) != DeserializationError::Ok) return;
    const char* tipo = evento["e"] | "";
    bool resolve = strcmp(tipo, caixa::nomeDoEvento(caixa::TipoEvento::TomadaNoHorario)) == 0 ||
                   strcmp(tipo, caixa::nomeDoEvento(caixa::TipoEvento::TomadaComAtraso)) == 0 ||
                   strcmp(tipo, caixa::nomeDoEvento(caixa::TipoEvento::Esquecida)) == 0;
    uint32_t dia = evento["d"] | 0u;
    if (resolve && dia + 1 >= hoje) motor.marcarResolvida(evento["m"] | 0, dia, evento["h"] | 0);
  });
}

}  // namespace armazenamento
