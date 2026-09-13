// --- Mapeamento dos Pinos de Entrada e Saida ---
const int PIN_SOLAR_LDR  = A0; // Leitura de irradiacao solar pelo LDR
const int PIN_BOILER_NTC = A1; // Leitura da temperatura simulada do boiler
const int PIN_LED_GREEN  = 7;  // Indica que a residencia opera por energia solar/bateria
const int PIN_LED_YELLOW = 9;  // Indica acionamento do ar-condicionado/climatizacao
const int PIN_LED_RED    = 10; // Sinaliza modulacao PWM do chuveiro eletrico

// --- Parametros de Temperatura e Logica ---
const float TEMP_DESEJADA = 37.0; // Temperatura alvo do banho (C)
const float TEMP_MINIMA   = 20.0; // Temperatura base da agua fria (C)

void setup() {
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);

  Serial.begin(9600);
  Serial.println("==========================================");
  Serial.println(" Casa Inteligente com Autoconsumo Solar ");
  Serial.println("==========================================");
}

void loop() {
  // 1. Leitura dos Sensores
  int solRaw = analogRead(PIN_SOLAR_LDR);
  int tempRaw = analogRead(PIN_BOILER_NTC);

  // Mapeamento das leituras analogicas para valores de engenharia
  int percentualSol = map(solRaw, 0, 1023, 0, 100);
  float tempBoiler  = map(tempRaw, 0, 1023, 15, 60); // Simula faixa de 15C a 60C

  // 2. Indicador de Sustentabilidade (LED Verde)
  digitalWrite(PIN_LED_GREEN, HIGH); // Alimentado pelas pilhas/placa solar

  // 3. Gestao Dinamica de Cargas (Ar-Condicionado / LED Amarelo)
  // Ativa a climatizacao apenas se a geracao solar for abundante (> 60%)
  if (percentualSol > 60) {
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    digitalWrite(PIN_LED_YELLOW, LOW);
  }

  // 4. Termostato Eletronico Inteligente do Chuveiro (PWM / LED Vermelho)
  int pwmChuveiro = 0;
  if (tempBoiler < TEMP_DESEJADA) {
    // Modulacao proporcional: quanto mais fria a agua do boiler, mais o chuveiro esquenta
    float razaoAquecimento = (TEMP_DESEJADA - tempBoiler) / (TEMP_DESEJADA - TEMP_MINIMA);
    razaoAquecimento = constrain(razaoAquecimento, 0.0, 1.0);
    pwmChuveiro = (int)(razaoAquecimento * 255.0);
  } else {
    pwmChuveiro = 0; // Agua do boiler ja esta a 37C ou mais -> Chuveiro DESLIGADO!
  }

  // Aplica o sinal PWM proporcional ao LED do chuveiro
  analogWrite(PIN_LED_RED, pwmChuveiro);

  // 5. Exibicao dos Dados no Monitor Serial
  Serial.print("Sol: ");
  Serial.print(percentualSol);
  Serial.print("% | Temp. Boiler: ");
  Serial.print(tempBoiler, 1);
  Serial.print("C | Chuveiro (Potencia PWM): ");
  Serial.print(map(pwmChuveiro, 0, 255, 0, 100));
  Serial.println("%");

  delay(500); // Atualizacao a cada meio segundo
}
