#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Instância do Display Nokia 5110 (CLK, DIN, DC, CE, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 11, 5, 4, 3);

const int PIN_SOLAR = A0;   // LDR: dia/noite
const int PIN_BATERIA = A1; // Divisor da bateria

// --- Expansor de I/O PCF8574 (I2C: A4=SDA, A5=SCL) -------------------------
// Os 8 LEDs agora são individuais (sem parear em paralelo) e ficam nos pinos
// P0-P7 do PCF8574, liberando D2/D6/D7/D8/D9 do Uno para os botões. O PCF8574
// aciona em nível baixo (dreno aberto ~25mA); LED: 5V — resistor 220R — anodo
// — catodo — pino do PCF8574. Escrever 0 = LED aceso, 1 = apagado.
#define PCF8574_ADDR 0x20

void escreverLeds(byte estadoLigados) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(~estadoLigados);
  Wire.endTransmission();
}

// --- Mapa dos 8 LEDs individuais -------------------------------------------
const byte B_JARDIM1  = 1 << 0; // Verde - iluminação externa
const byte B_JARDIM2  = 1 << 1; // Verde - iluminação externa
const byte B_SALA     = 1 << 2; // Amarelo - cômodo
const byte B_COZINHA  = 1 << 3; // Amarelo - cômodo
const byte B_QUARTO   = 1 << 4; // Amarelo - cômodo
const byte B_BANHEIRO = 1 << 5; // Amarelo - cômodo
const byte B_FORNO    = 1 << 6; // Vermelho - carga pesada (cozinha)
const byte B_CHUVEIRO = 1 << 7; // Vermelho - carga pesada (banheiro)
const byte TODOS_LEDS = 0xFF;
const int NUM_LEDS = 8;

// Consumo estimado por LED (mV, didático) — cargas pesadas (forno/chuveiro)
// pesam bem mais que uma simples lâmpada de cômodo, de propósito: é o que
// torna o "% do máximo" no display um dado interessante de comparar.
const unsigned int consumoPorLed[NUM_LEDS] = {
  800, 800,     // Jardim 1, Jardim 2
  1500,         // Sala
  1500,         // Cozinha
  1500,         // Quarto
  1500,         // Banheiro
  6000,         // Forno
  6000          // Chuveiro
};

unsigned long calcularConsumo_mV(byte estado) {
  unsigned long mv = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (estado & (1 << i)) mv += consumoPorLed[i];
  }
  return mv;
}

const unsigned long CONSUMO_MAX_mV = 800 + 800 + 1500 * 4 + 6000 * 2; // todos ligados

// --- Botões -----------------------------------------------------------------
// 1 botão por LED/par + 1 de reinício. INPUT_PULLUP: pressionado = LOW.
// D0/D1 (RX/TX) e A4/A5 (I2C do PCF8574) ficam de fora de propósito.
const int NUM_BOTOES_GRUPO = 5;
const int pinBotaoGrupo[NUM_BOTOES_GRUPO] = { 2, 6, 7, 8, 9 };
const byte maskGrupoBtn[NUM_BOTOES_GRUPO] = {
  B_JARDIM1 | B_JARDIM2,   // Jardim (as 2 luzes externas juntas)
  B_SALA,
  B_QUARTO,
  B_BANHEIRO | B_CHUVEIRO, // Banheiro + chuveiro juntos
  B_COZINHA | B_FORNO      // Cozinha + forno juntos
};
const char* msgLigadoBtn[NUM_BOTOES_GRUPO]    = { "Jardim aceso", "Sala acesa", "Quarto aceso", "Chuveiro ligado", "Forno ligado" };
const char* msgDesligadoBtn[NUM_BOTOES_GRUPO] = { "Jardim apagado", "Sala apagada", "Quarto apagado", "Chuveiro desligado", "Forno desligado" };

const int PIN_BOTAO_RESET = 10;

bool leituraAnteriorGrupo[NUM_BOTOES_GRUPO] = { HIGH, HIGH, HIGH, HIGH, HIGH };
bool estadoEstavelGrupo[NUM_BOTOES_GRUPO]   = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long debounceGrupo[NUM_BOTOES_GRUPO] = { 0, 0, 0, 0, 0 };

bool leituraAnteriorReset = HIGH;
bool estadoEstavelReset   = HIGH;
unsigned long debounceReset = 0;

const unsigned long DEBOUNCE_MS = 40;

// --- Tensão da bateria ------------------------------------------------------
// Divisor 100k/10k em A1, lido com a referência interna de 1,1V (o Arduino é
// alimentado pela própria bateria, então a referência padrão AVcc só daria
// uma razão constante em relação a si mesma, não a tensão real).
const float BAT_DIVISOR = 11.0; // (100k + 10k) / 10k
const float VREF_INTERNA = 1.1;

float lerTensaoBateria() {
  analogReference(INTERNAL);
  analogRead(PIN_BATERIA); // descarta: 1a leitura após trocar referência não é confiável
  delay(2);
  int bruto = analogRead(PIN_BATERIA);
  analogReference(DEFAULT);
  analogRead(PIN_SOLAR);   // assenta de volta pra referência de 5V antes do próximo uso
  return (bruto * VREF_INTERNA / 1023.0) * BAT_DIVISOR;
}

// --- Roteiro determinístico dia/noite ---------------------------------------
// Cada evento diz "a partir deste instante (ms, desde o início do ciclo do
// modo atual), os LEDs ficam neste estado". O tempo decorrido "dá a volta"
// (módulo) na duração total, então o roteiro se repete em loop.
struct Evento {
  unsigned long t;
  byte mascara;
  const char* label;
};

// Ciclo DIURNO: uso "descuidado", combinações fixas, terminando esquecendo
// luzes acesas pela casa (jardim + sala).
const Evento EVENTOS_DIA[] = {
  { 0,     TODOS_LEDS,                                                                    "Todos (base)" },
  { 15000, (byte)(B_JARDIM1 | B_JARDIM2),                                                  "So jardim" },
  { 20000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_COZINHA | B_QUARTO | B_BANHEIRO),     "Jardim+comodos" },
  { 25000, TODOS_LEDS,                                                                     "Todos ligados" },
  { 30000, (byte)(TODOS_LEDS & ~B_COZINHA),                                                "Sai cozinha" },
  { 31000, (byte)(TODOS_LEDS & ~B_COZINHA & ~B_FORNO),                                     "Sai forno" },
  { 32000, (byte)(TODOS_LEDS & ~B_COZINHA & ~B_FORNO & ~B_QUARTO),                         "Sai quarto" },
  { 33000, (byte)(TODOS_LEDS & ~B_COZINHA & ~B_FORNO & ~B_QUARTO & ~B_BANHEIRO),           "Sai banheiro" },
  { 34000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),                                         "Esqueceu aceso" },
};
const int N_EVENTOS_DIA = sizeof(EVENTOS_DIA) / sizeof(Evento);
const unsigned long DURACAO_DIA = 39000; // 34000 + 5s de espera antes de repetir

// Ciclo NOTURNO: uso "consciente" na bateria, andando de cômodo em cômodo e
// só apagando o anterior 1s depois de acender o próximo. Banheiro+chuveiro e
// cozinha+forno seguem a simulação de "entrar, usar, sair" (aparelho liga 3s
// depois do cômodo, e o cômodo só apaga 2s depois do aparelho desligar).
const Evento EVENTOS_NOITE[] = {
  { 0,     (byte)(B_JARDIM1),                                             "Jardim 1" },
  { 2000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),                        "Sala" },
  { 7000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_QUARTO),             "Indo p/ quarto" },
  { 8000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO),                      "Quarto" },
  { 12000, (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO | B_BANHEIRO),         "Banheiro" },
  { 13000, (byte)(B_JARDIM1 | B_JARDIM2 | B_BANHEIRO),                    "Banheiro" },
  { 15000, (byte)(B_JARDIM1 | B_JARDIM2 | B_BANHEIRO | B_CHUVEIRO),       "Chuveiro on" },
  { 25000, (byte)(B_JARDIM1 | B_JARDIM2 | B_BANHEIRO),                    "Chuveiro off" },
  { 27000, (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO),                      "Volta ao quarto" },
  { 32000, (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO | B_COZINHA),          "Cozinha+forno" },
  { 33000, (byte)(B_JARDIM1 | B_JARDIM2 | B_COZINHA),                     "Cozinha+forno" },
  { 35000, (byte)(B_JARDIM1 | B_JARDIM2 | B_COZINHA | B_FORNO),           "Forno ligado" },
  { 45000, (byte)(B_JARDIM1 | B_JARDIM2 | B_COZINHA),                     "Forno desligado" },
  { 47000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),                        "Sala (15s)" },
  { 62000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_COZINHA),            "Cozinha s/forno" },
  { 63000, (byte)(B_JARDIM1 | B_JARDIM2 | B_COZINHA),                     "Cozinha s/forno" },
  { 67000, (byte)(B_JARDIM1 | B_JARDIM2 | B_COZINHA | B_SALA),            "Sala" },
  { 68000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),                        "Sala" },
  { 72000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_QUARTO),             "Quarto" },
  { 73000, (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO),                      "Quarto" },
  { 77000, (byte)0,                                                       "Dormindo..." },
};
const int N_EVENTOS_NOITE = sizeof(EVENTOS_NOITE) / sizeof(Evento);
const unsigned long DURACAO_NOITE = 82000; // 77000 + 5s de espera antes de repetir

byte calcularEstadoTabela(bool modoNoturno, unsigned long decorrido, const char** labelOut) {
  const Evento* tabela = modoNoturno ? EVENTOS_NOITE : EVENTOS_DIA;
  int n = modoNoturno ? N_EVENTOS_NOITE : N_EVENTOS_DIA;
  unsigned long duracao = modoNoturno ? DURACAO_NOITE : DURACAO_DIA;
  unsigned long t = decorrido % duracao;

  int idx = 0;
  for (int i = 0; i < n; i++) {
    if (tabela[i].t <= t) idx = i; else break;
  }
  *labelOut = tabela[idx].label;
  return tabela[idx].mascara;
}

// --- Estado ao vivo -----------------------------------------------------
byte estadoAtual = 0;
const char* mensagemAtual = "Iniciando";

unsigned long cicloInicio = 0;       // referência t=0 do roteiro do modo atual
unsigned long standbyAte = 0;        // botão pausa o roteiro até aqui ("stand-by")
unsigned long standbyIniciadoEm = 0; // quando o stand-by atual começou (0 = não está em stand-by)
const unsigned long STANDBY_MS = 5000;

bool modoNoturnoAnterior = false;
bool primeiraLeitura = true;

unsigned long ultimaAtualizacaoDisplay = 0;
const unsigned long DISPLAY_INTERVALO_MS = 150;

// Reinicia o roteiro do modo atual do zero (troca de dia/noite ou botão reset).
void iniciarCiclo(bool modoNoturno) {
  cicloInicio = millis();
  standbyAte = 0;
  standbyIniciadoEm = 0;
  estadoAtual = calcularEstadoTabela(modoNoturno, 0, &mensagemAtual);
}

void lerBotoesDeGrupo(unsigned long agora) {
  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    bool leitura = digitalRead(pinBotaoGrupo[i]);
    if (leitura != leituraAnteriorGrupo[i]) debounceGrupo[i] = agora;

    if ((agora - debounceGrupo[i]) > DEBOUNCE_MS && leitura != estadoEstavelGrupo[i]) {
      estadoEstavelGrupo[i] = leitura;
      if (leitura == LOW) { // borda de descida = botão pressionado
        bool jaEmStandby = (agora < standbyAte);
        bool ligado = (estadoAtual & maskGrupoBtn[i]) != 0;

        if (ligado) estadoAtual &= ~maskGrupoBtn[i];
        else        estadoAtual |= maskGrupoBtn[i];

        standbyAte = agora + STANDBY_MS;
        if (!jaEmStandby) standbyIniciadoEm = agora;
        mensagemAtual = ligado ? msgDesligadoBtn[i] : msgLigadoBtn[i];
      }
    }
    leituraAnteriorGrupo[i] = leitura;
  }
}

void lerBotaoReset(unsigned long agora, bool modoNoturno) {
  bool leitura = digitalRead(PIN_BOTAO_RESET);
  if (leitura != leituraAnteriorReset) debounceReset = agora;

  if ((agora - debounceReset) > DEBOUNCE_MS && leitura != estadoEstavelReset) {
    estadoEstavelReset = leitura;
    if (leitura == LOW) {
      iniciarCiclo(modoNoturno);
    }
  }
  leituraAnteriorReset = leitura;
}

void atualizarDisplay(bool modoNoturno) {
  unsigned long consumo = calcularConsumo_mV(estadoAtual);
  float tensaoBateria = lerTensaoBateria();
  float percentualMax = (100.0 * consumo) / (float)CONSUMO_MAX_mV;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  display.setCursor(0, 0);
  display.print(modoNoturno ? F("NOTURNO") : F("DIURNO"));

  display.setCursor(0, 9);
  display.print(mensagemAtual);

  display.setCursor(0, 18);
  display.print(F("Cons:"));
  display.print(consumo);
  display.print(F("mV"));

  display.setCursor(0, 27);
  display.print(F("Bat:"));
  display.print(tensaoBateria, 2);
  display.print(F("V"));

  display.setCursor(0, 36);
  display.print(percentualMax, 0);
  display.print(F("% do max"));

  display.display();
}

void setup() {
  Wire.begin();
  escreverLeds(0); // tudo apagado

  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    pinMode(pinBotaoGrupo[i], INPUT_PULLUP);
  }
  pinMode(PIN_BOTAO_RESET, INPUT_PULLUP);

  display.begin();
  display.setContrast(50);
}

void loop() {
  unsigned long agora = millis();
  bool modoNoturno = (analogRead(PIN_SOLAR) <= 300);

  if (modoNoturno != modoNoturnoAnterior || primeiraLeitura) {
    iniciarCiclo(modoNoturno);
    modoNoturnoAnterior = modoNoturno;
    primeiraLeitura = false;
  }

  lerBotoesDeGrupo(agora);
  lerBotaoReset(agora, modoNoturno);

  // Fim do stand-by: "encolhe" a referência de tempo pelo tanto que ficou
  // pausado, para o roteiro continuar exatamente de onde parou.
  if (standbyIniciadoEm != 0 && agora >= standbyAte) {
    cicloInicio += (agora - standbyIniciadoEm);
    standbyIniciadoEm = 0;
  }

  if (agora >= standbyAte) {
    estadoAtual = calcularEstadoTabela(modoNoturno, agora - cicloInicio, &mensagemAtual);
  }
  // Em stand-by: mantém estadoAtual/mensagemAtual como o botão deixou.

  escreverLeds(estadoAtual);

  if (agora - ultimaAtualizacaoDisplay >= DISPLAY_INTERVALO_MS) {
    ultimaAtualizacaoDisplay = agora;
    atualizarDisplay(modoNoturno);
  }
}
