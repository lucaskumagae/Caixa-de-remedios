#include "painel.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <algorithm>

#include "armazenamento.h"
#include "relogio.h"
#include "segredos_ou_exemplo.h"

namespace painel {

namespace {
constexpr size_t MAX_REMEDIOS = 16;

WebServer servidor(80);
std::vector<caixa::Medicamento>* remedios = nullptr;
caixa::MotorDeDoses* motor = nullptr;

bool autorizado() {
  if (servidor.authenticate(PAINEL_USUARIO, PAINEL_SENHA)) return true;
  servidor.requestAuthentication(BASIC_AUTH, "Caixa de remedios");
  return false;
}

void responderJson(int status, const JsonDocument& doc) {
  String corpo;
  serializeJson(doc, corpo);
  servidor.send(status, "application/json", corpo);
}

void responderErro(int status, const std::string& mensagem) {
  JsonDocument doc;
  doc["erro"] = mensagem;
  responderJson(status, doc);
}

bool lerCorpo(JsonDocument& doc) {
  if (!servidor.hasArg("plain") || deserializeJson(doc, servidor.arg("plain")) != DeserializationError::Ok) {
    responderErro(400, "JSON inválido.");
    return false;
  }
  return true;
}

std::vector<caixa::Medicamento>::iterator buscarPorId() {
  uint16_t id = static_cast<uint16_t>(servidor.arg("id").toInt());
  return std::find_if(remedios->begin(), remedios->end(),
                      [id](const caixa::Medicamento& m) { return m.id == id; });
}

// Salva, avisa o motor e responde com a lista atualizada.
void aplicarMudanca() {
  if (!armazenamento::salvarRemedios(*remedios)) {
    responderErro(500, "Não foi possível salvar na memória da caixa.");
    return;
  }
  motor->definirAgenda(*remedios, relogio::horaValida() ? relogio::agoraLocal() : 0);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& m : *remedios) armazenamento::remedioParaJson(m, arr.add<JsonObject>());
  responderJson(200, doc);
}

void paginaInicial() {
  if (!autorizado()) return;
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    servidor.send(500, "text/plain",
                  "Pagina nao encontrada. Envie a pasta data com: pio run -t uploadfs");
    return;
  }
  servidor.streamFile(f, "text/html");
  f.close();
}

void estado() {
  if (!autorizado()) return;
  JsonDocument doc;
  doc["horaValida"] = relogio::horaValida();
  doc["agora"] = relogio::agoraLocal();
  doc["rtc"] = relogio::temRtc();
  doc["wifi"] = WiFi.status() == WL_CONNECTED;
  doc["ip"] = WiFi.localIP().toString();
  JsonArray ativas = doc["dosesAtivas"].to<JsonArray>();
  for (const auto& d : motor->dosesAtivas()) {
    JsonObject o = ativas.add<JsonObject>();
    o["medId"] = d.medId;
    o["compartimento"] = d.compartimento + 1;
    o["horario"] = caixa::horarioParaTexto(d.horario);
    o["tampaErrada"] = d.tampaErrada;
  }
  responderJson(200, doc);
}

void listarRemedios() {
  if (!autorizado()) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto& m : *remedios) armazenamento::remedioParaJson(m, arr.add<JsonObject>());
  responderJson(200, doc);
}

void criarRemedio() {
  if (!autorizado()) return;
  if (remedios->size() >= MAX_REMEDIOS) return responderErro(400, "Limite de 16 remédios atingido.");
  JsonDocument corpo;
  if (!lerCorpo(corpo)) return;
  caixa::Medicamento m;
  std::string erro = armazenamento::remedioDeJson(corpo.as<JsonObjectConst>(), m);
  if (!erro.empty()) return responderErro(400, erro);
  uint16_t maior = 0;
  for (const auto& r : *remedios) maior = std::max(maior, r.id);
  m.id = maior + 1;
  remedios->push_back(m);
  aplicarMudanca();
}

void editarRemedio() {
  if (!autorizado()) return;
  auto it = buscarPorId();
  if (it == remedios->end()) return responderErro(404, "Remédio não encontrado.");
  JsonDocument corpo;
  if (!lerCorpo(corpo)) return;
  caixa::Medicamento m;
  std::string erro = armazenamento::remedioDeJson(corpo.as<JsonObjectConst>(), m);
  if (!erro.empty()) return responderErro(400, erro);
  m.id = it->id;
  *it = m;
  aplicarMudanca();
}

void excluirRemedio() {
  if (!autorizado()) return;
  auto it = buscarPorId();
  if (it == remedios->end()) return responderErro(404, "Remédio não encontrado.");
  remedios->erase(it);
  aplicarMudanca();
}

void historico() {
  if (!autorizado()) return;
  size_t limite = servidor.hasArg("limite") ? servidor.arg("limite").toInt() : 100;
  limite = std::min<size_t>(std::max<size_t>(limite, 1), 300);
  JsonDocument doc;
  armazenamento::historicoParaJson(doc.to<JsonArray>(), limite);
  responderJson(200, doc);
}

// Permite acertar a hora pelo celular quando a caixa não tem RTC nem internet.
void acertarHora() {
  if (!autorizado()) return;
  JsonDocument corpo;
  if (!lerCorpo(corpo)) return;
  int64_t utc = corpo["utc"] | 0LL;
  if (utc < 1704067200LL) return responderErro(400, "Hora inválida.");
  relogio::acertar(static_cast<time_t>(utc));
  estado();
}
}  // namespace

void iniciar(std::vector<caixa::Medicamento>& listaRemedios, caixa::MotorDeDoses& motorDeDoses) {
  remedios = &listaRemedios;
  motor = &motorDeDoses;

  servidor.on("/", HTTP_GET, paginaInicial);
  servidor.on("/api/estado", HTTP_GET, estado);
  servidor.on("/api/remedios", HTTP_GET, listarRemedios);
  servidor.on("/api/remedios", HTTP_POST, criarRemedio);
  servidor.on("/api/remedios", HTTP_PUT, editarRemedio);
  servidor.on("/api/remedios", HTTP_DELETE, excluirRemedio);
  servidor.on("/api/historico", HTTP_GET, historico);
  servidor.on("/api/hora", HTTP_POST, acertarHora);
  servidor.onNotFound([] { servidor.send(404, "text/plain", "Nao encontrado"); });
  servidor.begin();
}

void atualizar() { servidor.handleClient(); }

}  // namespace painel
