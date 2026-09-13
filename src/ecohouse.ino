#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Instância do Display Nokia 5110 (CLK, DIN, DC, CE, RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 11, 5, 4, 3);

const int PIN_SOLAR = A0; // Sensor para verificar se é dia/noite

// --- Grupos de LEDs -------------------------------------------------------
// Pares que sempre acendem juntos (jardim, banheiro+chuveiro, cozinha+forno)
// dividem o mesmo pino, ligados em paralelo, liberando pinos para os botões.
enum Grupo { G_JARDIM, G_SALA, G_QUARTO, G_BANHEIRO, G_COZINHA, NUM_GRUPOS };

const int pinGrupo[NUM_GRUPOS]      = { 2, 6, 7, 8, 9 };
const int ledsPorGrupo[NUM_GRUPOS]  = { 2, 1, 1, 2, 2 };
const char* nomeGrupo[NUM_GRUPOS]   = { "Jardim", "Sala", "Quarto", "Banho+Chuv", "Cozinha+Forno" };

bool estadoGrupo[NUM_GRUPOS] = { false, false, false, false, false };

// --- Botões -----------------------------------------------------------------
// 1 botão por grupo + 1 botão de reinício do ciclo atual. INPUT_PULLUP:
// pressionado = LOW. Nenhum usa D0/D1 para não atrapalhar a gravação por USB.
const int pinBotaoGrupo[NUM_GRUPOS] = { A2, A3, A4, A5, 10 };
const int PIN_BOTAO_RESET = 12;

bool leituraAnteriorGrupo[NUM_GRUPOS] = { HIGH, HIGH, HIGH, HIGH, HIGH };
bool estadoEstavelGrupo[NUM_GRUPOS]   = { HIGH, HIGH, HIGH, HIGH, HIGH };
unsigned long debounceGrupo[NUM_GRUPOS] = { 0, 0, 0, 0, 0 };

bool leituraAnteriorReset = HIGH;
bool estadoEstavelReset   = HIGH;
unsigned long debounceReset = 0;

const unsigned long DEBOUNCE_MS = 40;

// Qualquer botão de grupo pausa o avanço automático por 5s ("stand-by");
// o toggle em si é imediato, só o avanço da sequência é que espera.
unsigned long standbyAte = 0;
const unsigned long STANDBY_MS = 5000;

// --- Consumo simulado ---------------------------------------------------
const int V_LED = 2000; // mV aproximados por LED aceso (estimativa didática)

unsigned long acumuladoDiurno_mV_s = 0;
unsigned long acumuladoNoturno_mV_s = 0;

String mensagemAtual = "Iniciando";

// --- Sequência noturna: tour pela casa, um cômodo "ocupado" por vez -------
const int TOUR_LEN = 7;
const Grupo tourGrupo[TOUR_LEN] = { G_JARDIM, G_SALA, G_QUARTO, G_BANHEIRO, G_COZINHA, G_SALA, G_QUARTO };
const unsigned long tourDuracao[TOUR_LEN] = { 5000, 4000, 4000, 15000, 4000, 4000, 4000 };
int tourPasso = 0;
unsigned long tourProximaTroca = 0;

// --- Sequência diurna: combinações aleatórias (demo de sol em excesso) ----
unsigned long diaProximaTroca = 0;
const unsigned long DIA_PASSO_MS = 5000;

bool modoNoturnoAnterior = false;
bool primeiraLeitura = true;

unsigned long ultimaAtualizacaoDisplay = 0;
const unsigned long DISPLAY_INTERVALO_MS = 150;

void apagarTudo() {
  for (int i = 0; i < NUM_GRUPOS; i++) estadoGrupo[i] = false;
}

void aplicarEstados() {
  for (int i = 0; i < NUM_GRUPOS; i++) {
    digitalWrite(pinGrupo[i], estadoGrupo[i] ? HIGH : LOW);
  }
}

int calcularConsumoAtual_mV() {
  int mv = 0;
  for (int i = 0; i < NUM_GRUPOS; i++) {
    if (estadoGrupo[i]) mv += V_LED * ledsPorGrupo[i];
  }
  return mv;
}

void iniciarTourNoturno() {
  apagarTudo();
  tourPasso = 0;
  estadoGrupo[tourGrupo[0]] = true;
  tourProximaTroca = millis() + tourDuracao[0];
  mensagemAtual = String(nomeGrupo[tourGrupo[0]]);
}

void avancarTourNoturno() {
  unsigned long agora = millis();
  if (agora < standbyAte) return;       // em stand-by: aguarda
  if (agora < tourProximaTroca) return; // ainda no mesmo passo

  Grupo passoAnterior = tourGrupo[tourPasso];
  tourPasso = (tourPasso + 1) % TOUR_LEN;
  Grupo passoAtual = tourGrupo[tourPasso];

  if (passoAnterior != passoAtual) estadoGrupo[passoAnterior] = false;
  estadoGrupo[passoAtual] = true;

  tourProximaTroca = agora + tourDuracao[tourPasso];
  mensagemAtual = String(nomeGrupo[passoAtual]);
}

void sortearComboDiurno() {
  for (int i = 0; i < NUM_GRUPOS; i++) estadoGrupo[i] = random(0, 2);
  mensagemAtual = "Combo aleatorio";
  diaProximaTroca = millis() + DIA_PASSO_MS;
}

void iniciarDiurno() {
  acumuladoDiurno_mV_s = 0;
  acumuladoNoturno_mV_s = 0;
  sortearComboDiurno();
}

void avancarDiurno() {
  unsigned long agora = millis();
  if (agora < standbyAte) return;
  if (agora < diaProximaTroca) return;
  sortearComboDiurno();
}

void reiniciarCicloAtual(bool modoNoturno) {
  if (modoNoturno) iniciarTourNoturno(); else iniciarDiurno();
  standbyAte = millis(); // sem pausa: recomeça já
}

void lerBotoesDeGrupo() {
  unsigned long agora = millis();
  for (int i = 0; i < NUM_GRUPOS; i++) {
    bool leitura = digitalRead(pinBotaoGrupo[i]);
    if (leitura != leituraAnteriorGrupo[i]) debounceGrupo[i] = agora;

    if ((agora - debounceGrupo[i]) > DEBOUNCE_MS && leitura != estadoEstavelGrupo[i]) {
      estadoEstavelGrupo[i] = leitura;
      if (estadoEstavelGrupo[i] == LOW) { // borda de descida = botão pressionado
        estadoGrupo[i] = !estadoGrupo[i];
        standbyAte = agora + STANDBY_MS;
        mensagemAtual = String(nomeGrupo[i]) + (estadoGrupo[i] ? " ON" : " OFF");
      }
    }
    leituraAnteriorGrupo[i] = leitura;
  }
}

void lerBotaoReset(bool modoNoturno) {
  unsigned long agora = millis();
  bool leitura = digitalRead(PIN_BOTAO_RESET);
  if (leitura != leituraAnteriorReset) debounceReset = agora;

  if ((agora - debounceReset) > DEBOUNCE_MS && leitura != estadoEstavelReset) {
    estadoEstavelReset = leitura;
    if (estadoEstavelReset == LOW) {
      reiniciarCicloAtual(modoNoturno);
      mensagemAtual = "Reiniciado";
    }
  }
  leituraAnteriorReset = leitura;
}

void atualizarDisplay(bool modoNoturno) {
  int consumo = calcularConsumoAtual_mV();
  unsigned long contribuicao = (unsigned long)consumo * DISPLAY_INTERVALO_MS / 1000;
  if (modoNoturno) acumuladoNoturno_mV_s += contribuicao;
  else acumuladoDiurno_mV_s += contribuicao;

  float economia = 0;
  if (acumuladoDiurno_mV_s > 0) {
    economia = (1.0 - ((float)acumuladoNoturno_mV_s / (float)acumuladoDiurno_mV_s)) * 100.0;
  }
  if (economia < 0) economia = 0;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  display.setCursor(0, 0);
  display.print(modoNoturno ? F("NOTURNO") : F("DIURNO"));

  display.setCursor(0, 10);
  display.print(mensagemAtual);

  display.setCursor(0, 22);
  display.print(F("Cons: "));
  display.print(consumo);
  display.print(F("mV"));

  display.setCursor(0, 32);
  display.print(F("Econ: "));
  display.print(economia, 0);
  display.print(F("%"));

  display.display();
}

void setup() {
  for (int i = 0; i < NUM_GRUPOS; i++) {
    pinMode(pinGrupo[i], OUTPUT);
    pinMode(pinBotaoGrupo[i], INPUT_PULLUP);
  }
  pinMode(PIN_BOTAO_RESET, INPUT_PULLUP);

  display.begin();
  display.setContrast(50);

  randomSeed(analogRead(A1)); // A1 fica livre, só usada como ruído p/ semente

  apagarTudo();
  aplicarEstados();
}

void loop() {
  unsigned long agora = millis();

  bool modoNoturno = (analogRead(PIN_SOLAR) <= 300);

  lerBotoesDeGrupo();
  lerBotaoReset(modoNoturno);

  if (modoNoturno != modoNoturnoAnterior || primeiraLeitura) {
    reiniciarCicloAtual(modoNoturno);
    modoNoturnoAnterior = modoNoturno;
    primeiraLeitura = false;
  }

  if (modoNoturno) avancarTourNoturno(); else avancarDiurno();

  aplicarEstados();

  if (agora - ultimaAtualizacaoDisplay >= DISPLAY_INTERVALO_MS) {
    ultimaAtualizacaoDisplay = agora;
    atualizarDisplay(modoNoturno);
  }
}
