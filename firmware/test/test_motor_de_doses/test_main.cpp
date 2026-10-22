// Testes das regras de negócio. Rodam no computador: pio test -e native
#include <unity.h>

#include "agenda.h"
#include "motor_de_doses.h"

using namespace caixa;

namespace {

// 2026-10-22 (quinta-feira) em hora local.
constexpr uint32_t DIA = 20748;
HoraLocal em(uint32_t dia, int hh, int mm, int ss = 0) {
  return static_cast<HoraLocal>(dia) * 86400 + hh * 3600 + mm * 60 + ss;
}

Medicamento remedio(uint16_t id, uint8_t compartimento, std::vector<uint16_t> horarios,
                    uint8_t dias = TODOS_OS_DIAS) {
  Medicamento m;
  m.id = id;
  m.nome = "Remedio " + std::to_string(id);
  m.compartimento = compartimento;
  m.horarios = horarios;
  m.diasSemana = dias;
  return m;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_dia_da_semana() {
  TEST_ASSERT_EQUAL(4, diaDaSemana(0));    // 1970-01-01 quinta
  TEST_ASSERT_EQUAL(4, diaDaSemana(DIA));  // 2026-10-22 quinta
}

void test_horario_texto() {
  TEST_ASSERT_EQUAL(510, horarioDeTexto("08:30"));
  TEST_ASSERT_EQUAL(0, horarioDeTexto("00:00"));
  TEST_ASSERT_EQUAL(-1, horarioDeTexto("24:00"));
  TEST_ASSERT_EQUAL(-1, horarioDeTexto("8:30"));
  TEST_ASSERT_EQUAL(-1, horarioDeTexto("08h30"));
  TEST_ASSERT_EQUAL_STRING("08:30", horarioParaTexto(510).c_str());
}

void test_validacao() {
  Medicamento m = remedio(1, 0, {480});
  TEST_ASSERT_EQUAL_STRING("", validar(m).c_str());
  m.compartimento = 4;
  TEST_ASSERT_FALSE(validar(m).empty());
  m = remedio(1, 0, {480, 480});
  TEST_ASSERT_FALSE(validar(m).empty());
  m = remedio(1, 0, {});
  TEST_ASSERT_FALSE(validar(m).empty());
  m = remedio(1, 0, {480}, 0);
  TEST_ASSERT_FALSE(validar(m).empty());
}

void test_toca_no_horario_e_acende_led_certo() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 2, {8 * 60})}, 0);

  TEST_ASSERT_TRUE(motor.atualizar(em(DIA, 7, 59, 59)).empty());
  TEST_ASSERT_FALSE(motor.buzzerLigado());

  auto ev = motor.atualizar(em(DIA, 8, 0));
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL(TipoEvento::AlarmeIniciado, ev[0].tipo);
  TEST_ASSERT_TRUE(motor.buzzerLigado());
  TEST_ASSERT_EQUAL(EstadoLed::Aceso, motor.led(2));
  TEST_ASSERT_EQUAL(EstadoLed::Apagado, motor.led(0));
}

void test_abrir_tampa_certa_para_alarme() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 2, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));

  auto ev = motor.tampaAberta(2, em(DIA, 8, 5));
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL(TipoEvento::TomadaNoHorario, ev[0].tipo);
  TEST_ASSERT_FALSE(motor.buzzerLigado());
  TEST_ASSERT_EQUAL(EstadoLed::Apagado, motor.led(2));
  // Não volta a tocar no mesmo minuto nem mais tarde no mesmo dia.
  TEST_ASSERT_TRUE(motor.atualizar(em(DIA, 8, 6)).empty());
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_toca_todos_os_dias() {
  // Regressão do bug da versão antiga: o alarme diário tocava só no primeiro dia.
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  for (uint32_t d = DIA; d < DIA + 7; ++d) {
    auto ev = motor.atualizar(em(d, 8, 0));
    TEST_ASSERT_EQUAL(1, ev.size());
    motor.tampaAberta(0, em(d, 8, 1));
    TEST_ASSERT_FALSE(motor.buzzerLigado());
  }
}

void test_aberta_com_atraso() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));
  auto ev = motor.tampaAberta(0, em(DIA, 8, 40));
  TEST_ASSERT_EQUAL(TipoEvento::TomadaComAtraso, ev[0].tipo);
}

void test_esquecida_depois_da_tolerancia() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));
  TEST_ASSERT_TRUE(motor.atualizar(em(DIA, 8, 59)).empty());
  TEST_ASSERT_TRUE(motor.buzzerLigado());

  auto ev = motor.atualizar(em(DIA, 9, 0));
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL(TipoEvento::Esquecida, ev[0].tipo);
  TEST_ASSERT_TRUE(ehAlerta(ev[0].tipo));
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_tampa_errada_continua_tocando_e_pisca_rapido() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 1, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));

  auto ev = motor.tampaAberta(3, em(DIA, 8, 2));
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL(TipoEvento::TampaErrada, ev[0].tipo);
  TEST_ASSERT_EQUAL(3, ev[0].compartimento);
  TEST_ASSERT_TRUE(motor.buzzerLigado());
  TEST_ASSERT_EQUAL(EstadoLed::PiscandoRapido, motor.led(1));
  TEST_ASSERT_EQUAL(EstadoLed::Apagado, motor.led(3));

  ev = motor.tampaAberta(1, em(DIA, 8, 3));
  TEST_ASSERT_EQUAL(TipoEvento::TomadaNoHorario, ev[0].tipo);
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_duas_doses_juntas_precisam_das_duas_tampas() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60}), remedio(2, 3, {8 * 60})}, 0);
  auto ev = motor.atualizar(em(DIA, 8, 0));
  TEST_ASSERT_EQUAL(2, ev.size());
  TEST_ASSERT_EQUAL(EstadoLed::Aceso, motor.led(0));
  TEST_ASSERT_EQUAL(EstadoLed::Aceso, motor.led(3));

  motor.tampaAberta(0, em(DIA, 8, 1));
  TEST_ASSERT_TRUE(motor.buzzerLigado());
  TEST_ASSERT_EQUAL(EstadoLed::Apagado, motor.led(0));
  TEST_ASSERT_EQUAL(EstadoLed::Aceso, motor.led(3));

  motor.tampaAberta(3, em(DIA, 8, 2));
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_abertura_fora_de_horario_gera_alerta() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  auto ev = motor.tampaAberta(0, em(DIA, 14, 0));
  TEST_ASSERT_EQUAL(1, ev.size());
  TEST_ASSERT_EQUAL(TipoEvento::AberturaForaDeHorario, ev[0].tipo);
  TEST_ASSERT_TRUE(ehAlerta(ev[0].tipo));
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_respeita_dias_da_semana() {
  // Só às segundas (bit 1). DIA é quinta; DIA + 4 é segunda.
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60}, 1u << 1)}, 0);
  TEST_ASSERT_TRUE(motor.atualizar(em(DIA, 8, 0)).empty());
  TEST_ASSERT_EQUAL(1, motor.atualizar(em(DIA + 4, 8, 0)).size());
}

void test_tolerancia_atravessa_meia_noite() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {23 * 60 + 50})}, 0);
  motor.atualizar(em(DIA, 23, 50));
  TEST_ASSERT_TRUE(motor.atualizar(em(DIA + 1, 0, 10)).empty());
  TEST_ASSERT_TRUE(motor.buzzerLigado());
  auto ev = motor.tampaAberta(0, em(DIA + 1, 0, 20));
  TEST_ASSERT_EQUAL(TipoEvento::TomadaComAtraso, ev[0].tipo);
  TEST_ASSERT_EQUAL(DIA, ev[0].dia);
}

void test_reboot_recupera_dose_perdida_mas_nao_repete_tomada() {
  // A energia caiu às 07:58 e voltou às 08:20: a dose das 08:00 ainda toca.
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  TEST_ASSERT_EQUAL(1, motor.atualizar(em(DIA, 8, 20)).size());

  // Reboot às 08:30 depois de a dose já ter sido tomada (lida do histórico).
  MotorDeDoses depois;
  depois.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  depois.marcarResolvida(1, DIA, 8 * 60);
  TEST_ASSERT_TRUE(depois.atualizar(em(DIA, 8, 30)).empty());
}

void test_cadastrar_remedio_atrasado_nao_toca_na_hora() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, em(DIA, 8, 10));
  TEST_ASSERT_TRUE(motor.atualizar(em(DIA, 8, 10)).empty());
  // No dia seguinte toca normalmente.
  TEST_ASSERT_EQUAL(1, motor.atualizar(em(DIA + 1, 8, 0)).size());
}

void test_excluir_remedio_para_alarme() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));
  TEST_ASSERT_TRUE(motor.buzzerLigado());
  motor.definirAgenda({}, em(DIA, 8, 1));
  TEST_ASSERT_FALSE(motor.buzzerLigado());
}

void test_mudar_compartimento_move_led() {
  MotorDeDoses motor;
  motor.definirAgenda({remedio(1, 0, {8 * 60})}, 0);
  motor.atualizar(em(DIA, 8, 0));
  motor.definirAgenda({remedio(1, 2, {8 * 60})}, em(DIA, 8, 1));
  TEST_ASSERT_EQUAL(EstadoLed::Apagado, motor.led(0));
  TEST_ASSERT_EQUAL(EstadoLed::Aceso, motor.led(2));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_dia_da_semana);
  RUN_TEST(test_horario_texto);
  RUN_TEST(test_validacao);
  RUN_TEST(test_toca_no_horario_e_acende_led_certo);
  RUN_TEST(test_abrir_tampa_certa_para_alarme);
  RUN_TEST(test_toca_todos_os_dias);
  RUN_TEST(test_aberta_com_atraso);
  RUN_TEST(test_esquecida_depois_da_tolerancia);
  RUN_TEST(test_tampa_errada_continua_tocando_e_pisca_rapido);
  RUN_TEST(test_duas_doses_juntas_precisam_das_duas_tampas);
  RUN_TEST(test_abertura_fora_de_horario_gera_alerta);
  RUN_TEST(test_respeita_dias_da_semana);
  RUN_TEST(test_tolerancia_atravessa_meia_noite);
  RUN_TEST(test_reboot_recupera_dose_perdida_mas_nao_repete_tomada);
  RUN_TEST(test_cadastrar_remedio_atrasado_nao_toca_na_hora);
  RUN_TEST(test_excluir_remedio_para_alarme);
  RUN_TEST(test_mudar_compartimento_move_led);
  return UNITY_END();
}
