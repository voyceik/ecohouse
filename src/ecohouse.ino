#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Instância do Display Nokia 5110 (CLK, DIN, DC, CE, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 11, 5, 4, 3);

const int PIN_SOLAR = A0; // Sensor para verificar se é dia/noite

// Definição dos Pinos dos 8 LEDs
const int LED_JARDIM1  = 2;
const int LED_JARDIM2  = 6;
const int LED_SALA     = 7;
const int LED_COZINHA  = 8;
const int LED_QUARTO   = 9;
const int LED_BANHEIRO = 10;
const int LED_FORNO    = 12;
const int LED_CHUVEIRO = A1; // Pino analógico usado como digital

// Tensões aproximadas para cálculo do consumo simulado (mV por LED aceso)
const int V_VERDE_AMARELO = 2000;
const int V_VERMELHO      = 1800;

unsigned long acumuladoDiurno_mV_s = 0;
unsigned long acumuladoNoturno_mV_s = 0;

void apagarTodos() {
  digitalWrite(LED_JARDIM1, LOW);
  digitalWrite(LED_JARDIM2, LOW);
  digitalWrite(LED_SALA, LOW);
  digitalWrite(LED_COZINHA, LOW);
  digitalWrite(LED_QUARTO, LOW);
  digitalWrite(LED_BANHEIRO, LOW);
  digitalWrite(LED_FORNO, LOW);
  digitalWrite(LED_CHUVEIRO, LOW);
}

int calcularConsumoAtual_mV() {
  int mv = 0;
  if (digitalRead(LED_JARDIM1))  mv += V_VERDE_AMARELO;
  if (digitalRead(LED_JARDIM2))  mv += V_VERDE_AMARELO;
  if (digitalRead(LED_SALA))     mv += V_VERDE_AMARELO;
  if (digitalRead(LED_COZINHA))  mv += V_VERDE_AMARELO;
  if (digitalRead(LED_QUARTO))   mv += V_VERDE_AMARELO;
  if (digitalRead(LED_BANHEIRO)) mv += V_VERDE_AMARELO;
  if (digitalRead(LED_FORNO))    mv += V_VERMELHO;
  if (digitalRead(LED_CHUVEIRO)) mv += V_VERMELHO;
  return mv;
}

// Função aceita strings salvas na Flash via F()
void atualizarPainel(const __FlashStringHelper* msg, int tempoSegundos, bool isNoturno) {
  unsigned long inicio = millis();
  unsigned long duracao = (unsigned long)tempoSegundos * 1000;

  while (millis() - inicio < duracao) {
    int consumoInstantaneo = calcularConsumoAtual_mV();

    if (isNoturno) {
      acumuladoNoturno_mV_s += (consumoInstantaneo / 10);
    } else {
      acumuladoDiurno_mV_s += (consumoInstantaneo / 10);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);

    display.setCursor(0, 0);
    display.print(isNoturno ? F("MODO: NOTURNO") : F("MODO: DIURNO"));

    display.setCursor(0, 12);
    display.print(msg);

    display.setCursor(0, 28);
    display.print(F("Cons: "));
    display.print(consumoInstantaneo);
    display.print(F(" mV"));

    display.display();
    delay(100);
  }
}

void setup() {
  pinMode(LED_JARDIM1, OUTPUT);
  pinMode(LED_JARDIM2, OUTPUT);
  pinMode(LED_SALA, OUTPUT);
  pinMode(LED_COZINHA, OUTPUT);
  pinMode(LED_QUARTO, OUTPUT);
  pinMode(LED_BANHEIRO, OUTPUT);
  pinMode(LED_FORNO, OUTPUT);
  pinMode(LED_CHUVEIRO, OUTPUT);

  display.begin();
  display.setContrast(50);
  apagarTodos();
}

void loop() {
  int nivelSolar = analogRead(PIN_SOLAR);

  // SE FOR DIA (Placa recebendo luz)
  if (nivelSolar > 300) {
    acumuladoDiurno_mV_s = 0;
    acumuladoNoturno_mV_s = 0;

    // 1. Acende todos por 15 segundos
    digitalWrite(LED_JARDIM1, HIGH);
    digitalWrite(LED_JARDIM2, HIGH);
    digitalWrite(LED_SALA, HIGH);
    digitalWrite(LED_COZINHA, HIGH);
    digitalWrite(LED_QUARTO, HIGH);
    digitalWrite(LED_BANHEIRO, HIGH);
    digitalWrite(LED_FORNO, HIGH);
    digitalWrite(LED_CHUVEIRO, HIGH);
    atualizarPainel(F("Todos LIG"), 15, false);

    // 2. Apaga todos e fica só o jardim (5 seg)
    apagarTodos();
    digitalWrite(LED_JARDIM1, HIGH);
    digitalWrite(LED_JARDIM2, HIGH);
    atualizarPainel(F("So jardim"), 5, false);

    // 3. Jardim + cômodos (5 seg)
    digitalWrite(LED_SALA, HIGH);
    digitalWrite(LED_COZINHA, HIGH);
    digitalWrite(LED_QUARTO, HIGH);
    digitalWrite(LED_BANHEIRO, HIGH);
    atualizarPainel(F("Jard+Comodos"), 5, false);

    // 4. Forno + Chuveiro "Todos" (5 seg)
    digitalWrite(LED_FORNO, HIGH);
    digitalWrite(LED_CHUVEIRO, HIGH);
    atualizarPainel(F("Todos"), 5, false);

    // 5. Simulação de esquecimento pelo caminho
    apagarTodos();
    digitalWrite(LED_JARDIM1, HIGH);
    digitalWrite(LED_JARDIM2, HIGH);
    digitalWrite(LED_SALA, HIGH);
    atualizarPainel(F("Esquec: Sala"), 5, false);

    digitalWrite(LED_COZINHA, HIGH);
    atualizarPainel(F("Esquec: Coz"), 5, false);

    digitalWrite(LED_FORNO, HIGH);
    atualizarPainel(F("Esquec: Forno"), 5, false);

    digitalWrite(LED_QUARTO, HIGH);
    atualizarPainel(F("Esquec: Quarto"), 5, false);

    digitalWrite(LED_BANHEIRO, HIGH);
    atualizarPainel(F("Esquec: Banh"), 5, false);

    digitalWrite(LED_CHUVEIRO, HIGH);
    atualizarPainel(F("Esquec: Chuv"), 5, false);

    apagarTodos();
  }
  // SE FOR NOITE (Operação por bateria - Consumo Consciente)
  else {
    // 1. Jardim (uma e depois outra)
    apagarTodos();
    digitalWrite(LED_JARDIM1, HIGH);
    atualizarPainel(F("Jardim 1"), 5, true);

    digitalWrite(LED_JARDIM2, HIGH);
    atualizarPainel(F("Jardim 1+2"), 1, true);
    digitalWrite(LED_JARDIM1, LOW);
    atualizarPainel(F("Jardim 2"), 4, true);

    // 2. Sala
    digitalWrite(LED_SALA, HIGH);
    atualizarPainel(F("Indo p/ Sala"), 1, true);
    digitalWrite(LED_JARDIM2, LOW);
    atualizarPainel(F("Na Sala"), 4, true);

    // 3. Quarto
    digitalWrite(LED_QUARTO, HIGH);
    atualizarPainel(F("Indo p/ Quarto"), 1, true);
    digitalWrite(LED_SALA, LOW);
    atualizarPainel(F("No Quarto"), 4, true);

    // 4. Banheiro + Chuveiro (15s)
    digitalWrite(LED_BANHEIRO, HIGH);
    digitalWrite(LED_CHUVEIRO, HIGH);
    atualizarPainel(F("Indo Banho"), 1, true);
    digitalWrite(LED_QUARTO, LOW);
    atualizarPainel(F("Banho 15s"), 15, true);

    // 5. Volta para o Quarto
    digitalWrite(LED_QUARTO, HIGH);
    atualizarPainel(F("Volta Quarto"), 1, true);
    digitalWrite(LED_BANHEIRO, LOW);
    digitalWrite(LED_CHUVEIRO, LOW);
    atualizarPainel(F("No Quarto"), 4, true);

    // 6. Cozinha + Forno
    digitalWrite(LED_COZINHA, HIGH);
    digitalWrite(LED_FORNO, HIGH);
    atualizarPainel(F("Indo Cozinha"), 1, true);
    digitalWrite(LED_QUARTO, LOW);
    atualizarPainel(F("Cozinha+Forno"), 4, true);

    // 7. Sala por 15 segundos
    digitalWrite(LED_SALA, HIGH);
    atualizarPainel(F("Indo p/ Sala"), 1, true);
    digitalWrite(LED_COZINHA, LOW);
    digitalWrite(LED_FORNO, LOW);
    atualizarPainel(F("Sala 15s"), 15, true);

    // 8. Cozinha sem forno
    digitalWrite(LED_COZINHA, HIGH);
    atualizarPainel(F("Indo Cozinha"), 1, true);
    digitalWrite(LED_SALA, LOW);
    atualizarPainel(F("Cozinha s/Forno"), 4, true);

    // 9. Sala
    digitalWrite(LED_SALA, HIGH);
    atualizarPainel(F("Indo Sala"), 1, true);
    digitalWrite(LED_COZINHA, LOW);
    atualizarPainel(F("Na Sala"), 4, true);

    // 10. Quarto
    digitalWrite(LED_QUARTO, HIGH);
    atualizarPainel(F("Indo Quarto"), 1, true);
    digitalWrite(LED_SALA, LOW);
    atualizarPainel(F("No Quarto"), 4, true);

    // 11. Apaga tudo (Dormindo)
    apagarTodos();

    // 12. Exibe o percentual de economia
    float economia = 0;
    if (acumuladoDiurno_mV_s > 0) {
      economia = (1.0 - ((float)acumuladoNoturno_mV_s / (float)acumuladoDiurno_mV_s)) * 100.0;
    }
    if (economia < 0) economia = 0;

    unsigned long telaInicio = millis();
    while (millis() - telaInicio < 10000) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(F("FIM DO CICLO"));

      display.setCursor(0, 16);
      display.print(F("ECONOMIA:"));

      display.setTextSize(2);
      display.setCursor(0, 28);
      display.print(economia, 1);
      display.print(F("%"));

      display.display();
      delay(200);

      if (analogRead(PIN_SOLAR) > 300) break;
    }
  }
}
