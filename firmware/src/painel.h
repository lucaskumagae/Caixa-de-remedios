// Painel web servido pela própria caixa: página em /index.html (LittleFS) e API JSON.
#pragma once

#include <vector>

#include "agenda.h"
#include "motor_de_doses.h"

namespace painel {

void iniciar(std::vector<caixa::Medicamento>& remedios, caixa::MotorDeDoses& motor);
void atualizar();

}  // namespace painel
