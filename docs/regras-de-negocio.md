# Regras de negócio

A caixa lembra a pessoa na hora certa, insiste até ela abrir o compartimento certo, registra se a dose foi tomada e mostra no histórico o que aconteceu.

## Princípios

1. **A caixa funciona sozinha.** Agenda, relógio e histórico ficam no ESP32. Celular ou computador servem só para programar.
2. **Wi‑Fi é opcional.** Sem a rede da casa, a caixa continua tocando; o painel fica disponível na rede própria da caixa.
3. **Uma caixa, um paciente.**
4. **Só os dados necessários:** nome do remédio, dose, horários e histórico.

## Compartimentos

- A caixa tem 4 compartimentos com tampas individuais.
- Cada compartimento tem um sensor de tampa e um LED.
- Cada remédio é cadastrado em um compartimento. Mais de um remédio pode ficar no mesmo compartimento.

## Alarme

| Situação | O que acontece |
| --- | --- |
| Chegou o horário | Buzzer bipa e o LED do compartimento do remédio acende |
| Tampa certa aberta até 15 min depois | Para o alarme daquele remédio; registra **tomada no horário** |
| Tampa certa aberta entre 15 e 60 min depois | Para o alarme; registra **tomada com atraso** |
| Ninguém abriu em 60 min | Alarme para; registra **esquecida** (alerta) |
| Tampa errada aberta durante o alarme | Alarme continua e o LED certo pisca rápido; registra **tampa errada** (alerta) |
| Duas doses no mesmo horário | Os dois LEDs acendem; o alarme só para quando as duas tampas forem abertas |
| Tampa aberta sem alarme tocando | Nada toca; registra **aberta fora de horário** (alerta) |
| Caixa sem hora certa | Todos os LEDs dão uma piscada curta a cada 2 s; o painel pede para acertar a hora |

Os alertas aparecem em vermelho no histórico do painel.

Os tempos de 15 e 60 minutos ficam em `firmware/include/config.h` (`REGRAS`).

## Casos de borda

- **Queda de energia:** ao religar, uma dose cujo horário passou há menos de 60 min ainda toca. Uma dose já tomada ou esquecida (registrada no histórico) não toca de novo.
- **Cadastro em cima da hora:** cadastrar às 08:10 um remédio das 08:00 não dispara o alarme na hora; ele passa a tocar a partir do próximo horário.
- **Remédio excluído ou alterado durante o alarme:** o alarme daquela dose para. Se só o compartimento mudou, o LED passa para o compartimento novo.
- **Dose perto da meia-noite:** a tolerância de 60 min continua valendo depois da virada do dia.
- **Dias da semana:** cada remédio pode tocar só em alguns dias.

## Hora

Ordem de preferência:

1. RTC DS3231 (com bateria, mantém a hora sem energia e sem internet).
2. NTP, quando a caixa está na rede da casa. A hora do NTP também corrige o RTC.
3. Botão "Acertar hora agora" no painel, que usa a hora do celular.

Fuso fixo de Brasília (UTC−3).

## Fora do escopo

- Servidor em computador (PHP/MySQL).
- Cadastro de vários usuários na caixa.
- Pastas ou arquivos de receitas.
- Modo não perturbe que silencia o alarme.
