// Persistência na flash do ESP32 (LittleFS):
//   /remedios.json    agenda de remédios
//   /historico.jsonl  um evento por linha; ao passar do limite vira /historico.old
#pragma once

#include <ArduinoJson.h>

#include <vector>

#include "agenda.h"
#include "motor_de_doses.h"

namespace armazenamento {

bool iniciar();

std::vector<caixa::Medicamento> carregarRemedios();
bool salvarRemedios(const std::vector<caixa::Medicamento>& remedios);

// Conversão entre o JSON do painel e o Medicamento. O compartimento no JSON vai de 1 a 4.
void remedioParaJson(const caixa::Medicamento& m, JsonObject saida);
// Retorna mensagem de erro ou string vazia.
std::string remedioDeJson(JsonObjectConst entrada, caixa::Medicamento& m);

void registrar(const caixa::Evento& evento, const std::string& nomeRemedio);
// Últimos `limite` eventos, do mais recente para o mais antigo.
void historicoParaJson(JsonArray saida, size_t limite);
// Marca no motor as doses de ontem e hoje que já foram tomadas ou esquecidas,
// para não tocarem de novo depois de um reinício.
void carregarResolvidas(caixa::MotorDeDoses& motor, uint32_t hoje);

}  // namespace armazenamento
