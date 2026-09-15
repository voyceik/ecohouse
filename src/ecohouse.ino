#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

// Instância do Display Nokia 5110 (CLK, DIN, DC, CE, RST) — D8~D12
Adafruit_PCD8544 display = Adafruit_PCD8544(8, 9, 10, 11, 12);

// LDR: dia/noite. Só 2 fios (A0 e GND) — o pull-up interno do pino faz o
// papel do resistor fixo (5V --pull-up-- A0 --LDR-- GND). Como o LDR fica do
// lado do GND (e não do 5V como num divisor "normal"), a lógica é invertida:
// luz = LDR com pouca resistência = A0 puxado pra baixo; escuro = LDR com
// muita resistência = A0 sobe perto de 5V. Por isso "noturno" é leitura ALTA.
const int PIN_SOLAR = A0;
// Histerese: dois limiares em vez de um só, pra sombra/ruído perto do meio
// não ficar trocando de modo. Só entra no noturno com leitura BEM alta
// (sensor de fato coberto) e só volta pro diurno com leitura BEM baixa
// (sensor de fato na luz); entre os dois, mantém o modo atual. Pull-up
// interno do ATmega328 varia (datasheet: 20-50k), então esses valores são
// só um ponto de partida — calibre com Serial.println(analogRead(PIN_SOLAR))
// na luz normal e com o sensor coberto, e ajuste.
const int LIMIAR_ENTRA_NOTURNO = 950; // leitura >= isso: escurece o bastante p/ virar noite
const int LIMIAR_SAI_NOTURNO   = 500; // leitura <= isso: clareia o bastante p/ virar dia
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
const byte B_JARDIM1  = 1 << 6; // Verde - iluminação externa
const byte B_JARDIM2  = 1 << 7; // Verde - iluminação externa
const byte B_SALA     = 1 << 3; //Azul - cômodo
const byte B_COZINHA  = 1 << 2; //Azul - cômodo
const byte B_QUARTO   = 1 << 4; //Azul - cômodo
const byte B_BANHEIRO = 1 << 5; //Azul - cômodo
const byte B_FORNO    = 1 << 0; // Vermelho - carga pesada (cozinha)
const byte B_CHUVEIRO = 1 << 1; // Vermelho - carga pesada (banheiro)
const byte TODOS_LEDS = 0xFF;
const int NUM_LEDS = 8;

// Consumo estimado por carga (W, potência real aproximada) — forno e chuveiro
// são as cargas pesadas da casa, bem acima de uma simples lâmpada de cômodo:
// é o que torna o "% do máximo" no display um dado interessante de comparar.
const unsigned int consumoPorLed[NUM_LEDS] = {
  2000,       
  3000,      
  50,        
  50,        
  50,        
  50,        
  50,       
  50     
};

unsigned long calcularConsumo_W(byte estado) {
  unsigned long w = 0;
  for (int i = 0; i < NUM_LEDS; i++) {
    if (estado & (1 << i)) w += consumoPorLed[i];
  }
  return w;
}

const unsigned long CONSUMO_MAX_W = 50 * 6 + 2000 + 3000; // todos ligados

// --- Botões -----------------------------------------------------------------
// 1 botão por LED/par + 1 de reinício, todos em D2~D7 (contíguos). INPUT_PULLUP:
// pressionado = LOW. D0/D1 (RX/TX) e A4/A5 (I2C do PCF8574) ficam de fora de propósito.
const int NUM_BOTOES_GRUPO = 5;
const int pinBotaoGrupo[NUM_BOTOES_GRUPO] = { 2, 3, 4, 5, 6 };
const byte maskGrupoBtn[NUM_BOTOES_GRUPO] = {
  B_JARDIM1 | B_JARDIM2,   // Jardim (as 2 luzes externas juntas)
  B_SALA,
  B_QUARTO,
  B_BANHEIRO | B_CHUVEIRO, // Banheiro + chuveiro juntos
  B_COZINHA | B_FORNO      // Cozinha + forno juntos
};
// Strings PROGMEM nomeadas (não dá pra usar F() direto num inicializador
// global — a macro expande pra um statement-expression, só permitido dentro
// de função). Assim o texto fica só na flash; os arrays abaixo guardam só os
// ponteiros (2 bytes cada) em RAM. Com o roteiro noturno maior, manter tudo
// isso como "const char*" comum apontando pra literais quase estourou os
// 2KB de RAM do Uno (95%+), a ponto de corromper o buffer do display.
const char BMSG_JARDIM_ON[]    PROGMEM = "Jardim (lig)";
const char BMSG_SALA_ON[]      PROGMEM = "Sala (lig)";
const char BMSG_QUARTO_ON[]    PROGMEM = "Quarto (lig)";
const char BMSG_BANHEIRO_ON[]  PROGMEM = "Banheiro (lig)";
const char BMSG_COZINHA_ON[]   PROGMEM = "Cozinha (lig)";
const char BMSG_JARDIM_OFF[]   PROGMEM = "Jardim (desl)";
const char BMSG_SALA_OFF[]     PROGMEM = "Sala (desl)";
const char BMSG_QUARTO_OFF[]   PROGMEM = "Quarto (desl)";
const char BMSG_BANHEIRO_OFF[] PROGMEM = "Banheiro(desl)";
const char BMSG_COZINHA_OFF[]  PROGMEM = "Cozinha (desl)";
const char BMSG_VAZIO[]        PROGMEM = "";
const char BMSG_BANHO[]        PROGMEM = "Chuveiro (lig)";
const char BMSG_FORNO[]        PROGMEM = "Forno (lig)";

const char* const msgLigadoBtn[NUM_BOTOES_GRUPO]    = { BMSG_JARDIM_ON, BMSG_SALA_ON, BMSG_QUARTO_ON, BMSG_BANHEIRO_ON, BMSG_COZINHA_ON };
const char* const msgDesligadoBtn[NUM_BOTOES_GRUPO] = { BMSG_JARDIM_OFF, BMSG_SALA_OFF, BMSG_QUARTO_OFF, BMSG_BANHEIRO_OFF, BMSG_COZINHA_OFF };
// Banheiro e cozinha (índices 3 e 4) não são um toggle simples: ligar entra
// num ciclo que se repete sozinho até apertar de novo. msgUsoBtn é a mensagem
// da 2a fase do ciclo (chuveiro/forno ligados); só é usada nesses dois índices.
const char* const msgUsoBtn[NUM_BOTOES_GRUPO] = { BMSG_VAZIO, BMSG_VAZIO, BMSG_VAZIO, BMSG_BANHO, BMSG_FORNO };

// Ciclo dos botões "pesados" (banheiro+chuveiro, cozinha+forno): ao ligar,
// acende só a luz do ambiente por CICLO_ENTRADA_MS, depois soma a carga
// pesada (chuveiro/forno) por CICLO_USO_MS, e repete indefinidamente até o
// botão ser pressionado de novo (aí desliga os dois LEDs do grupo).
const unsigned long CICLO_ENTRADA_MS = 2000;
const unsigned long CICLO_USO_MS     = 12000;
const unsigned long CICLO_PERIODO_MS = CICLO_ENTRADA_MS + CICLO_USO_MS;
const byte maskAmbientePesado[NUM_BOTOES_GRUPO] = { 0, 0, 0, B_BANHEIRO, B_COZINHA };
const byte maskCargaPesada[NUM_BOTOES_GRUPO]    = { 0, 0, 0, B_CHUVEIRO, B_FORNO };
bool ehBotaoPesado(int i) { return i == 3 || i == 4; }

bool cicloPesadoAtivo[NUM_BOTOES_GRUPO]           = { false, false, false, false, false };
unsigned long cicloPesadoInicio[NUM_BOTOES_GRUPO] = { 0, 0, 0, 0, 0 };
bool cicloPesadoFaseUso[NUM_BOTOES_GRUPO]         = { false, false, false, false, false };

const int PIN_BOTAO_RESET = 7;

// Botão de emergência: segurar o botão do Jardim (índice 0, D2) por
// REBOOT_SEGURAR_MS reinicia o Arduino de verdade via watchdog (não é só um
// "pular pro início do código" — o watchdog reseta o chip inteiro, incluindo
// periféricos como I2C/SPI, que é o que normalmente precisa ser limpo quando
// o sistema trava ou entra num estado bugado). Serve pra recuperar de
// qualquer mau funcionamento sem precisar tirar o cabo USB.
unsigned long jardimSeguradoDesde = 0; // 0 = não está sendo segurado agora
const unsigned long REBOOT_SEGURAR_MS = 5000;

void verificarBotaoEmergencia(unsigned long agora) {
  bool pressionado = digitalRead(pinBotaoGrupo[0]) == LOW; // botão Jardim
  if (!pressionado) {
    jardimSeguradoDesde = 0;
    return;
  }
  if (jardimSeguradoDesde == 0) {
    jardimSeguradoDesde = agora;
  } else if (agora - jardimSeguradoDesde >= REBOOT_SEGURAR_MS) {
    wdt_enable(WDTO_15MS);
    while (true) {} // trava aqui até o watchdog estourar e reiniciar o chip
  }
}

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
// A maquete só tem uma bateria de ~12V no divisor, mas um banco off-grid de
// verdade é montado com várias em série (ex.: 10x12V = 120V). Escalamos a
// leitura por esse fator só para exibir uma tensão de banco mais realista.
const float FATOR_BANCO_BATERIAS = 10.0;

float lerTensaoBateria() {
  analogReference(INTERNAL);
  analogRead(PIN_BATERIA); // descarta: 1a leitura após trocar referência não é confiável
  delay(2);
  int bruto = analogRead(PIN_BATERIA);
  analogReference(DEFAULT);
  analogRead(PIN_SOLAR);   // assenta de volta pra referência de 5V antes do próximo uso
  return (bruto * VREF_INTERNA / 1023.0) * BAT_DIVISOR * FATOR_BANCO_BATERIAS;
}

// --- Roteiro determinístico (só à noite) -------------------------------------
// De dia o comportamento é simples (ver iniciarCiclo/loop): começa tudo
// ligado e cada botão só liga/desliga o seu grupo, sem roteiro automático.
// À noite ainda existe um roteiro com script fixo por horário: cada evento
// diz "a partir deste instante (ms, desde o início do ciclo), os LEDs ficam
// neste estado". O tempo decorrido "dá a volta" (módulo) na duração total,
// então o roteiro se repete em loop.
struct Evento {
  unsigned long t;
  byte mascara;
  const char* label; // ponteiro PROGMEM (ver EN_* abaixo); imprimir só via cast (const __FlashStringHelper*)
};

// Rótulos do roteiro noturno em PROGMEM (mesmo motivo do MSG_BANNER_INICIAL:
// como "const char*" comum, essas 29 strings sozinhas quase estouravam a RAM).
const char EN00[] PROGMEM = "Chegar em Casa";
const char EN01[] PROGMEM = "Abrir a porta";
const char EN02[] PROGMEM = "Entrar na Sala";
const char EN03[] PROGMEM = "Ir p/ o Quarto";
const char EN04[] PROGMEM = "No Quarto";
const char EN05[] PROGMEM = "Ir p/ Banheiro";
const char EN06[] PROGMEM = "No Banheiro";
const char EN07[] PROGMEM = "Tomar um banho";
const char EN08[] PROGMEM = "Enxugar-se";
const char EN09[] PROGMEM = "Voltar Quarto";
const char EN10[] PROGMEM = "Vestir pijama";
const char EN11[] PROGMEM = "Ir p/ a Sala";
const char EN12[] PROGMEM = "Ir p/ Cozinha";
const char EN13[] PROGMEM = "Ligar o Forno";
const char EN14[] PROGMEM = "Servir a janta";
const char EN15[] PROGMEM = "Jantar";
const char EN16[] PROGMEM = "Ir p/ Cozinha";
const char EN17[] PROGMEM = "Lavar louça";
const char EN18[] PROGMEM = "Indo p/ sala";
const char EN19[] PROGMEM = "Ouvir musica";
const char EN20[] PROGMEM = "Separar lixo";
const char EN21[] PROGMEM = "Por na lixeira";
const char EN22[] PROGMEM = "Voltar p/ Sala";
const char EN23[] PROGMEM = "Ir p/ Quarto";
const char EN24[] PROGMEM = "Ir p/ Banheiro";
const char EN25[] PROGMEM = "Escovar dentes";
const char EN26[] PROGMEM = "Ir p/ Quarto";
const char EN27[] PROGMEM = "Dormindo...";
const char EN28[] PROGMEM = "Boa noite !!!";

// Ciclo NOTURNO: uso "consciente" na bateria, andando de cômodo em cômodo e
// só apagando o anterior 1s depois de acender o próximo. Banheiro+chuveiro e
// cozinha+forno seguem a simulação de "entrar, usar, sair" (aparelho liga 3s
// depois do cômodo, e o cômodo só apaga 2s depois do aparelho desligar).
const Evento EVENTOS_NOITE[] = {
  { 0,     (byte)(B_JARDIM1),                                 EN00 },
  { 3000,  (byte)(B_JARDIM1 | B_JARDIM2),                     EN01 },
  { 5000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),            EN02 },
  { 7000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_QUARTO), EN03 },
  { 8000,  (byte)(B_JARDIM1 | B_JARDIM2 | B_QUARTO),          EN04 },
  { 12000, (byte)(B_JARDIM2 | B_QUARTO | B_BANHEIRO),         EN05 },
  { 13000, (byte)(B_JARDIM2 | B_BANHEIRO),                    EN06 },
  { 15000, (byte)(B_BANHEIRO | B_CHUVEIRO),                   EN07 },
  { 25000, (byte)(B_BANHEIRO),                                EN08 },
  { 27000, (byte)(B_BANHEIRO | B_QUARTO),                     EN09 },
  { 27000, (byte)(B_QUARTO),                                  EN10 },
  { 32000, (byte)(B_QUARTO | B_SALA),                         EN11 },
  { 34000, (byte)(B_SALA | B_COZINHA),                        EN12 },
  { 36000, (byte)(B_COZINHA | B_FORNO),                       EN13 },
  { 46000, (byte)(B_COZINHA | B_SALA),                        EN14 },
  { 48000, (byte)(B_SALA),                                    EN15 },
  { 58000, (byte)(B_SALA | B_COZINHA),                        EN16 },
  { 60000, (byte)(B_COZINHA),                                 EN17 },
  { 65000, (byte)(B_COZINHA | B_SALA),                        EN18 },
  { 67000, (byte)(B_SALA),                                    EN19 },
  { 77000, (byte)(B_JARDIM2 | B_SALA),                        EN20 },
  { 82000, (byte)(B_JARDIM1 | B_JARDIM2),                     EN21 },
  { 85000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA),            EN22 },
  { 87000, (byte)(B_JARDIM1 | B_JARDIM2 | B_SALA | B_QUARTO), EN23 },
  { 89000, (byte)(B_QUARTO | B_BANHEIRO),                     EN24 },
  { 91000, (byte)(B_BANHEIRO),                                EN25 },
  { 96000, (byte)(B_QUARTO | B_BANHEIRO),                     EN26 },
  { 98000, (byte)(B_QUARTO),                                  EN27 },
  { 100000, (byte)0,                                          EN28 },
};
const int N_EVENTOS_NOITE = sizeof(EVENTOS_NOITE) / sizeof(Evento);
const unsigned long DURACAO_NOITE = 110000; // 77000 + 5s de espera antes de repetir

byte calcularEstadoNoite(unsigned long decorrido, const char** labelOut) {
  unsigned long t = decorrido % DURACAO_NOITE;

  int idx = 0;
  for (int i = 0; i < N_EVENTOS_NOITE; i++) {
    if (EVENTOS_NOITE[i].t <= t) idx = i; else break;
  }
  *labelOut = EVENTOS_NOITE[idx].label;
  return EVENTOS_NOITE[idx].mascara;
}

// Banner de abertura (tela ociosa, sem roteiro rodando): a fonte padrão do
// display só cobre ASCII puro, então o texto vai sem acentos (senão os
// caracteres acentuados saem como glifo errado na tela). PROGMEM porque em
// RAM (2KB no Uno) esse texto sozinho já estourava o limite.
//
// Já vem quebrado à mão em linhas de exatamente 14 caracteres (84px/6px),
// preenchidas com espaços até o fim — cada string abaixo é uma "linha" da
// tela. Assim obterLinhaBanner() só precisa fatiar em blocos fixos de 14
// (sem nenhuma lógica de quebra de palavra em runtime) e nunca corta uma
// palavra no meio, porque a quebra já foi decidida aqui, palavra por
// palavra. Se for editar o texto, mantenha cada linha com 14 caracteres
// (conte os espaços de preenchimento no fim).
const char MSG_BANNER_INICIAL[] PROGMEM =
  "  ECO HOUSE   "
  "              "
  "Usar placas   "
  "solares em sua"
  "casa alem de  "
  "proteger o    "
  "nosso planeta "
  "e construir um"
  "amanha mais   "
  "sustentavel,  "
  "tambem ajuda a"
  "economizar no "
  "final das suas"
  "contas.       "
  "              "
  "  ECO HOUSE   "
  "              "
  "As Pequenas   "
  "atitudes      "
  "dentro de casa"
  "tais como, nao"
  "deixar todas  "
  "as luzes      "
  "acessas e nao "
  "demorar muito "
  "no chuveiro   "
  "nem deixar o  "
  "forno eletrico"
  "ligado sem    "
  "necessidade,  "
  "fazem toda    "
  "diferenca.    "
  "              "
  "  ECO HOUSE   "
  "              "
  "Dispositivos  "
  "inteligentes  "
  "otimizam o uso"
  "da energia    "
  "eletrica e    "
  "reduz o custo "
  "mensal.       "
  "              "
  "  ECO HOUSE   "
  "              "
  "Off grid é o  "
  "uso de painel "
  "solar com o   "
  "armazenamento "
  "em baterias p/"
  "usar a energia"
  "quando o sol  "
  "nao fornece   "
  "mais energia  "
  "suficiente.";

// Tela ociosa (banner): 1a linha (14 caracteres = 84px/6px) parada por
// OCIOSO_INTRO_MS com tudo apagado; depois "rola" o texto de baixo pra
// cima, 1 linha nova a cada OCIOSO_SCROLL_MS_POR_LINHA, usando as
// 6 linhas da tela inteira (48px/8px) — sem disputar espaço com
// Consumo/Bateria/% (que só voltam quando sai do modo ocioso).
// totalLinhasBanner é calculado 1x no setup().
const int BANNER_CHARS_POR_LINHA = 14;
const unsigned long OCIOSO_INTRO_MS = 3000;
const unsigned long OCIOSO_SCROLL_MS_POR_LINHA = 1500;
const int LINHAS_TELA = 6; // 48px de altura / 8px por linha de texto
int totalLinhasBanner = 0;

void obterLinhaBanner(int indice, char* buf) {
  int total = strlen_P(MSG_BANNER_INICIAL);
  int inicio = indice * BANNER_CHARS_POR_LINHA;
  int i = 0;
  for (; i < BANNER_CHARS_POR_LINHA && (inicio + i) < total; i++) {
    buf[i] = pgm_read_byte(MSG_BANNER_INICIAL + inicio + i);
  }
  buf[i] = '\0';
}

// --- Estado ao vivo -----------------------------------------------------
byte estadoAtual = 0;
const char MSG_INICIANDO[] PROGMEM = "MOBIPE";
const char MSG_ECO_HOUSE[] PROGMEM = "PAINEL SOLAR";
const char* mensagemAtual = MSG_INICIANDO; // ponteiro PROGMEM; imprimir só via cast (const __FlashStringHelper*)

unsigned long cicloInicio = 0;       // referência t=0 do roteiro do modo atual
unsigned long standbyAte = 0;        // botão pausa o roteiro até aqui ("stand-by")
unsigned long standbyIniciadoEm = 0; // quando o stand-by atual começou (0 = não está em stand-by)
const unsigned long STANDBY_MS = 5000;

bool modoNoturno = false;         // estado com memória (histerese) — ver loop()
bool modoNoturnoAnterior = false;
bool primeiraLeitura = true;

// true = o roteiro noturno já completou uma volta inteira (chegou em
// "Dormindo..." e esperou o resto de DURACAO_NOITE). A partir daí o roteiro
// para de rodar e a tela/LEDs ficam no modo ocioso (banner ECOHOUSE) mesmo
// que o LDR ainda leia noite — só volta a rodar o roteiro quando o LDR
// realmente virar dia, ou o botão de reset for apertado.
bool noiteEncerrada = false;

// Modo ocioso = tela do banner ECOHOUSE (sem roteiro noturno nem botão
// mudando nada "manualmente" há um tempo). Ativo ao entrar em modo diurno,
// ao terminar o roteiro noturno (noiteEncerrada) e de volta após
// INATIVIDADE_DIA_MS sem nenhum botão ser apertado nesse estado.
bool modoOcioso = true;
bool ociosoEmScroll = false;       // false = 1a linha parada; true = rolando
unsigned long ociosoFaseInicio = 0; // t=0 da fase atual (parada ou rolagem)
unsigned long ultimaInteracaoDia = 0;
const unsigned long INATIVIDADE_DIA_MS = 10000;

unsigned long ultimaAtualizacaoDisplay = 0;
const unsigned long DISPLAY_INTERVALO_MS = 150;

// Reinicia o estado do modo atual (troca de dia/noite ou botão reset).
// De dia: volta pro banner ocioso com tudo apagado. De noite: reinicia o
// roteiro do zero (t=0).
void iniciarCiclo(bool modoNoturno) {
  cicloInicio = millis();
  standbyAte = 0;
  standbyIniciadoEm = 0;
  noiteEncerrada = false;
  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    cicloPesadoAtivo[i] = false;
    cicloPesadoFaseUso[i] = false;
  }
  if (modoNoturno) {
    estadoAtual = calcularEstadoNoite(0, &mensagemAtual);
    modoOcioso = false;
  } else {
    estadoAtual = 0;
    mensagemAtual = MSG_ECO_HOUSE;
    modoOcioso = true;
    ociosoEmScroll = false;
    ociosoFaseInicio = cicloInicio; // = millis(), setado acima
  }
}

void lerBotoesDeGrupo(unsigned long agora, bool modoNoturno) {
  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    bool leitura = digitalRead(pinBotaoGrupo[i]);
    if (leitura != leituraAnteriorGrupo[i]) debounceGrupo[i] = agora;

    if ((agora - debounceGrupo[i]) > DEBOUNCE_MS && leitura != estadoEstavelGrupo[i]) {
      estadoEstavelGrupo[i] = leitura;
      if (leitura == LOW) { // borda de descida = botão pressionado
        if (i == 0 && modoOcioso && !ociosoEmScroll) {
          // Jardim fica "desligado" enquanto o banner ocioso ainda está na
          // intro parada (antes de começar a rolagem): evita que um toque
          // ainda preso no pino logo após um reboot (ex.: o próprio botão de
          // emergência, segurado até o reset) seja lido como um novo toque e
          // pule direto pro modo dia/noite sem passar pelo banner.
          leituraAnteriorGrupo[i] = leitura;
          continue;
        }
        ultimaInteracaoDia = agora;
        if (modoOcioso) {
          if (modoNoturno) {
            // Sai do banner ocioso indo pro modo noturno: o botão só
            // "acorda" o sistema — quem manda nos LEDs a partir daqui é o
            // roteiro noturno (calcularEstadoNoite), que roda até completar
            // a volta inteira antes de o loop() voltar pro banner sozinho.
            // Este toque não teve efeito de toggle: ignora o resto do botão.
            iniciarCiclo(true);
            leituraAnteriorGrupo[i] = leitura;
            continue;
          }
          // Sai do banner ocioso: apaga tudo e entra no modo diurno normal;
          // o toggle abaixo já aplica o efeito deste botão em cima do "tudo apagado".
          modoOcioso = false;
          estadoAtual = 0;
        }

        bool jaEmStandby = (agora < standbyAte);
        standbyAte = agora + STANDBY_MS;
        if (!jaEmStandby) standbyIniciadoEm = agora;

        if (ehBotaoPesado(i)) {
          // Banheiro/cozinha: não é toggle instantâneo, é liga/desliga do ciclo
          // (ver aplicarCiclosPesados). Desligar aqui já apaga os dois LEDs na hora.
          cicloPesadoAtivo[i] = !cicloPesadoAtivo[i];
          if (cicloPesadoAtivo[i]) {
            cicloPesadoInicio[i] = agora;
            cicloPesadoFaseUso[i] = false;
            mensagemAtual = msgLigadoBtn[i];
          } else {
            estadoAtual &= ~(maskAmbientePesado[i] | maskCargaPesada[i]);
            mensagemAtual = msgDesligadoBtn[i];
          }
        } else {
          bool ligado = (estadoAtual & maskGrupoBtn[i]) != 0;
          if (ligado) estadoAtual &= ~maskGrupoBtn[i];
          else        estadoAtual |= maskGrupoBtn[i];
          mensagemAtual = ligado ? msgDesligadoBtn[i] : msgLigadoBtn[i];
        }
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

// Roda todo loop: para cada botão pesado ligado, decide a fase pelo tempo
// decorrido desde que ligou (módulo CICLO_PERIODO_MS, então repete sozinho) e
// força os bits correspondentes em estadoAtual — isso vale tanto de dia quanto
// de noite, sobrepondo o roteiro noturno enquanto o ciclo estiver ativo.
void aplicarCiclosPesados(unsigned long agora) {
  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    if (!ehBotaoPesado(i) || !cicloPesadoAtivo[i]) continue;

    unsigned long faseT = (agora - cicloPesadoInicio[i]) % CICLO_PERIODO_MS;
    bool faseUso = faseT >= CICLO_ENTRADA_MS;

    if (faseUso) {
      estadoAtual |= (maskAmbientePesado[i] | maskCargaPesada[i]);
    } else {
      estadoAtual |= maskAmbientePesado[i];
      estadoAtual &= ~maskCargaPesada[i];
    }

    if (faseUso != cicloPesadoFaseUso[i]) {
      cicloPesadoFaseUso[i] = faseUso;
      mensagemAtual = faseUso ? msgUsoBtn[i] : msgLigadoBtn[i];
    }
  }
}

// Roda a cada loop enquanto modoOcioso estiver ativo: controla as duas fases
// do banner (1a linha parada / rolagem) e o efeito de LEDs piscando aleatório
// durante a rolagem.
void avancarOcioso(unsigned long agora) {
  unsigned long decorridoFase = agora - ociosoFaseInicio;

  if (!ociosoEmScroll) {
    estadoAtual = 0;
    if (decorridoFase >= OCIOSO_INTRO_MS) {
      ociosoEmScroll = true;
      ociosoFaseInicio = agora;
      estadoAtual = 0;
    }
  } else {
    static unsigned long proximoFlicker = 0;
    if (agora >= proximoFlicker) {
      int idx = random(NUM_LEDS);
      estadoAtual ^= (byte)(1 << idx); // liga/desliga esse LED (depende do estado atual dele)
      proximoFlicker = agora + random(300, 900);
    }

    long scrollLinha = decorridoFase / OCIOSO_SCROLL_MS_POR_LINHA;
    if (scrollLinha >= totalLinhasBanner + (LINHAS_TELA - 1)) {
      // última linha já saiu por completo do topo da tela: reinicia do começo
      ociosoEmScroll = false;
      ociosoFaseInicio = agora;
      estadoAtual = 0;
    }
  }
}

void desenharOcioso(unsigned long agora) {
  char linha[BANNER_CHARS_POR_LINHA + 1];

  if (!ociosoEmScroll) {
    obterLinhaBanner(0, linha);
    display.setCursor(0, 0);
    display.print(linha);
  } else {
    unsigned long decorridoFase = agora - ociosoFaseInicio;
    long scrollLinha = decorridoFase / OCIOSO_SCROLL_MS_POR_LINHA;
    for (int r = 0; r < LINHAS_TELA; r++) {
      long idx = scrollLinha - (LINHAS_TELA - 1) + r;
      if (idx >= 0 && idx < totalLinhasBanner) {
        obterLinhaBanner(idx, linha);
        display.setCursor(0, r * 8);
        display.print(linha);
      }
    }
  }
}

void atualizarDisplay(bool modoNoturno, int percentualSolar) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  if (modoOcioso) {
    desenharOcioso(millis());
  } else {
    unsigned long consumo = calcularConsumo_W(estadoAtual);
    float tensaoBateria = lerTensaoBateria();
    float percentualMax = (100.0 * consumo) / (float)CONSUMO_MAX_W;

    display.setCursor(0, 0);
    display.print(modoNoturno ? F("NOTURNO ") : F("DIURNO "));
    display.print(percentualSolar);
    display.print('%');

    display.setCursor(0, 9);
    display.print((const __FlashStringHelper*)mensagemAtual); // ponteiro é PROGMEM (ver EN_*/BMSG_*/MSG_*)

    display.setCursor(0, 18);
    display.print(F("Consumo:"));
    display.print(consumo);
    display.print(F("W"));

    display.setCursor(0, 27);
    display.print(F("Baterias:"));
    display.print(tensaoBateria, 2);
    display.print(F("V"));

    display.setCursor(0, 36);
    display.print(percentualMax, 0);
    display.print(F("% de Energia "));
  }

  display.display();
}

void setup() {
  wdt_disable(); // garante que um reset anterior não deixe o watchdog armado num loop de reboot

  Serial.begin(9600); // debug: ver leitura do LDR e o modo no Serial Monitor
  Wire.begin();
  escreverLeds(0); // tudo apagado

  pinMode(PIN_SOLAR, INPUT_PULLUP); // pull-up interno faz o divisor com o LDR
  randomSeed(analogRead(PIN_SOLAR)); // ruído do pino flutuante como semente

  for (int i = 0; i < NUM_BOTOES_GRUPO; i++) {
    pinMode(pinBotaoGrupo[i], INPUT_PULLUP);
  }
  pinMode(PIN_BOTAO_RESET, INPUT_PULLUP);

  totalLinhasBanner = (strlen_P(MSG_BANNER_INICIAL) + BANNER_CHARS_POR_LINHA - 1) / BANNER_CHARS_POR_LINHA;

  display.begin();
  display.setContrast(55); // ajuste conforme o painel; 100 pode ficar todo branco (em branco)
  display.setRotation(2); // gira 180 graus (display montado de cabeça pra baixo)
  display.setTextWrap(false); // sem isso, print() de texto comprido invade as linhas de baixo
}

void loop() {
  unsigned long agora = millis();

  verificarBotaoEmergencia(agora);

  int leituraSolar = analogRead(PIN_SOLAR);

  // 0% = leitura no limiar de virar noite; 99% = leitura ~0 (LDR no limite de
  // baixa resistência, luz máxima). Mostrado no cabeçalho do display tanto de
  // dia quanto de noite; à noite a leitura fica acima do limiar, então cai em
  // 0% (constrain abaixo) — reflete fielmente que está bem escuro.
  int percentualSolar = map(leituraSolar, LIMIAR_ENTRA_NOTURNO, 0, 0, 99);
  percentualSolar = constrain(percentualSolar, 0, 99);

  // Entra no noturno só quando o % exibido chega mesmo a 0 (em vez de um
  // limiar bruto separado) — assim o display nunca mostra "DIURNO 0%" por
  // alguns segundos antes de virar noite de fato.
  if (percentualSolar == 0) modoNoturno = true;
  else if (leituraSolar <= LIMIAR_SAI_NOTURNO) modoNoturno = false;
  // entre os dois limiares: mantém o modo atual (zona morta da histerese)

  // Debug: valor bruto do LDR + limiares, 1x/segundo. Serve pra calibrar
  // LIMIAR_ENTRA_NOTURNO/LIMIAR_SAI_NOTURNO — abra o Serial Monitor (9600
  // baud), cubra/descubra o LDR e compare a leitura com os dois limiares.
  static unsigned long ultimoDebug = 0;
  if (agora - ultimoDebug >= 1000) {
    ultimoDebug = agora;
    Serial.print(F("LDR="));
    Serial.print(leituraSolar);
    Serial.print(F(" (entra_noturno>="));
    Serial.print(LIMIAR_ENTRA_NOTURNO);
    Serial.print(F(", sai_noturno<="));
    Serial.print(LIMIAR_SAI_NOTURNO);
    Serial.print(F(") modoNoturno="));
    Serial.println(modoNoturno ? F("NOTURNO") : F("DIURNO"));
  }

  if (modoNoturno != modoNoturnoAnterior || primeiraLeitura) {
    iniciarCiclo(modoNoturno);
    modoNoturnoAnterior = modoNoturno;
    primeiraLeitura = false;
  }

  lerBotoesDeGrupo(agora, modoNoturno);
  lerBotaoReset(agora, modoNoturno);

  if (modoNoturno && !noiteEncerrada) {
    // Fim do stand-by: "encolhe" a referência de tempo pelo tanto que ficou
    // pausado, para o roteiro continuar exatamente de onde parou.
    if (standbyIniciadoEm != 0 && agora >= standbyAte) {
      cicloInicio += (agora - standbyIniciadoEm);
      standbyIniciadoEm = 0;
    }

    if (agora - cicloInicio >= DURACAO_NOITE) {
      // Roteiro completou uma volta inteira ("Dormindo..." + a espera final):
      // para de rodar e cai no modo ocioso (banner) até o LDR virar dia de
      // verdade ou o botão de reset reiniciar o roteiro.
      noiteEncerrada = true;
      modoOcioso = true;
      ociosoEmScroll = false;
      ociosoFaseInicio = agora;
      estadoAtual = 0;
    } else if (agora >= standbyAte) {
      estadoAtual = calcularEstadoNoite(agora - cicloInicio, &mensagemAtual);
    }
    // Em stand-by: mantém estadoAtual/mensagemAtual como o botão deixou.
  } else {
    // De dia, ou noite já encerrada: sem roteiro automático. Ou roda o banner
    // ocioso, ou (se um botão já tirou da tela ociosa) volta pra ele sozinho
    // depois de INATIVIDADE_DIA_MS sem nenhum botão ser apertado.
    if (modoOcioso) {
      avancarOcioso(agora);
    } else if (agora - ultimaInteracaoDia >= INATIVIDADE_DIA_MS) {
      modoOcioso = true;
      ociosoEmScroll = false;
      ociosoFaseInicio = agora;
      estadoAtual = 0;
    }
  }

  aplicarCiclosPesados(agora);

  escreverLeds(estadoAtual);

  if (agora - ultimaAtualizacaoDisplay >= DISPLAY_INTERVALO_MS) {
    ultimaAtualizacaoDisplay = agora;
    atualizarDisplay(modoNoturno, percentualSolar);
  }
}
