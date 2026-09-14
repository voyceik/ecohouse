# Casa Inteligente com Autoconsumo Solar

Maquete de casa sustentável com Arduino Uno, display Nokia 5110 (PCD8544, monocromático, sem touch), 8 LEDs individuais representando cômodos/aparelhos e 6 botões de controle manual, simulando um roteiro fixo de consumo de energia ao longo de um ciclo dia/noite.

## Compilar e enviar (PlatformIO)

Projeto configurado para PlatformIO (`platformio.ini`), alvo `uno` (Arduino Uno R3). A porta está fixada em `COM8` — ajuste em `platformio.ini` se a placa aparecer em outra porta.

```
pio run                 # compila
pio run --target upload # compila e grava na placa
```

O sketch também pode ser aberto direto no Arduino IDE a partir de `src/ecohouse.ino`. Não usa nenhuma lib nova além do Nokia 5110 — o expansor de LEDs é controlado via `Wire.h` (já vem com o Arduino core).

## Hardware

- Arduino Uno R3 + cabo USB
- Protoboard 400 furos + jumpers macho-macho
- Mini placa solar 5V/200mA + placa controladora de carga (bornes S+/S-, B+/B-, L+/L-)
- Suporte 4 pilhas AA + 4 pilhas recarregáveis Ni-MH (4,8V nominal)
- LDR 5516 + resistor 10kΩ (divisor de tensão, sensor dia/noite)
- Display Nokia 5110 (PCD8544, monocromático, sem touch)
- **Expansor de I/O PCF8574** (I2C) — necessário para os 8 LEDs individuais sem esgotar os pinos digitais do Uno
- 8 LEDs individuais (dos 15 disponíveis: 2 verdes, 4 amarelos, 2 vermelhos) + resistores 220Ω
- 6 botões grandes (5 de grupo + 1 de reinício), sem resistor externo (usa o pull-up interno do Arduino)
- Resistores 100kΩ + 10kΩ (divisor de tensão para medir a bateria em A1)

## Ligações

**Placa controladora solar**
- S+/S- → mini placa solar (+/-)
- B+/B- → suporte de pilhas (+/-)
- L- → GND do Arduino / trilha terra da protoboard
- L+ → trilha +5V da protoboard / pino 5V do Arduino

**Sensor de luz (entrada analógica)**
- LDR solar (dia/noite): +5V — LDR — A0 — resistor 10kΩ — GND

**Tensão da bateria (entrada analógica)** — ⚠️ ainda não montado fisicamente, o firmware já espera por ele
- Divisor: B+/L+ (positivo da bateria) — resistor 100kΩ — nó — A1 — resistor 10kΩ — GND
- O nó entre os dois resistores é o que vai para A1. Sem esse divisor, a leitura de tensão no display não tem significado (pino flutuando).
- Por quê: o Arduino é alimentado pela própria bateria (via L+/L- do controlador), então medir com a referência padrão (5V/AVcc) só daria uma razão constante em relação a si mesma. O código usa a referência interna de 1,1V do ATmega328P para medir de verdade — daí o divisor precisar reduzir a tensão da bateria (até uns 5,8V) para bem abaixo de 1,1V antes de chegar em A1.

**Display Nokia 5110 (SPI)**
| Pino do display | Pino do Arduino |
|---|---|
| CLK  | D8  |
| DIN  | D9  |
| DC   | D10 |
| CE   | D11 |
| RST  | D12 |
| VCC  | 3,3V (não 5V) |
| GND  | GND |
| BL (luz de fundo) | 3,3V ou GND via resistor, conforme o módulo |

**Expansor PCF8574 (I2C)**
| Pino do PCF8574 | Pino do Arduino |
|---|---|
| SDA | A4 |
| SCL | A5 |
| VCC | 5V |
| GND | GND |
| A0, A1, A2 (endereço) | GND (endereço 0x20) |

**LEDs individuais** (cada LED: 5V → resistor 220Ω → anodo; catodo → pino do PCF8574. O PCF8574 aciona em nível baixo, "puxando" o catodo pra 0V: escrever 0 acende, 1 apaga)
| LED | Pino do PCF8574 (P0-P7) | Cor | Uso | Potência simulada |
|---|---|---|---|---|
| Jardim 1 | P0 | Verde | Iluminação externa | 50 W |
| Jardim 2 | P1 | Verde | Iluminação externa | 50 W |
| Sala | P2 | Amarelo | Cômodo | 50 W |
| Cozinha | P3 | Amarelo | Cômodo | 50 W |
| Quarto | P4 | Amarelo | Cômodo | 50 W |
| Banheiro | P5 | Amarelo | Cômodo | 50 W |
| Forno | P6 | Vermelho | Carga pesada (cozinha) | 2000 W |
| Chuveiro | P7 | Vermelho | Carga pesada (banheiro) | 3000 W |

**Botões** (uma perna no pino, outra no GND — sem resistor, usa `INPUT_PULLUP`)
| Botão | Pino | Ação |
|---|---|---|
| Jardim | D2 | Liga/desliga Jardim 1 + Jardim 2 juntos |
| Sala | D3 | Liga/desliga Sala |
| Quarto | D4 | Liga/desliga Quarto |
| Banheiro + Chuveiro | D5 | Liga/desliga os dois juntos |
| Cozinha + Forno | D6 | Liga/desliga os dois juntos |
| Reinício | D7 | Reinicia do zero o ciclo atual (dia ou noite) |

D0/D1 (RX/TX) não são usados, para não atrapalhar a gravação por USB. A4/A5 são dedicados ao I2C do PCF8574. Pinos digitais em ordem: **D2~D7 = botões**, **D8~D12 = display Nokia** (CLK, DIN, DC, CE, RST).

## Lógica

**Sensor de luz (A0) > 300 → modo DIURNO**: simples e manual, sem roteiro automático. Ao entrar no modo (ou apertar reinício), **acende todos os 8 LEDs**; a partir daí cada botão só liga/desliga o seu grupo, e o estado fica exatamente como os botões deixaram até o próximo reinício ou troca pra modo noturno.

**Sensor de luz (A0) ≤ 300 → modo NOTURNO** ("uso consciente", rodando na bateria): **roteiro fixo por horário** — uma tabela de eventos com o instante (ms desde o início do ciclo) e o estado dos 8 LEDs naquele instante; o `millis()` decorrido "dá a volta" na duração total do roteiro, repetindo-o. Anda de cômodo em cômodo e só apaga o anterior 1s depois de acender o próximo:
1. Jardim 1, depois Jardim 2 (as duas luzes externas ficam acesas o resto do ciclo, e só apagam no passo final).
2. Sala.
3. Quarto.
4. Banheiro + chuveiro — simula alguém tomando banho: o chuveiro liga só 3s depois do banheiro, fica 10s ligado e o banheiro apaga 2s depois do chuveiro desligar (ciclo de 15s no total).
5. Volta para o quarto.
6. Cozinha + forno — mesma simulação de "entrar, usar, sair": forno liga 3s depois da cozinha, fica 10s ligado e a cozinha apaga 2s depois (15s no total).
7. Sala por 15s.
8. Cozinha sem forno (só a luz, sem "cozinhar").
9. Sala, depois quarto.
10. Apaga tudo, inclusive o jardim — "dormindo" — e o ciclo recomeça do passo 1.

**Simulação de chuveiro/forno**: em qualquer ponto do roteiro em que o cômodo liga junto com o aparelho pesado (banheiro→chuveiro, cozinha→forno), o aparelho não acende junto — ele espera 3s (a "pessoa entrou no cômodo"), fica ligado 10s (o "banho"/"uso do forno") e o cômodo em si só apaga 2s depois do aparelho desligar (a "pessoa sai").

**Qualquer botão de grupo**: alterna o(s) LED(s) daquele grupo na hora (liga/desliga simples) — os outros LEDs mantêm o estado atual. A mensagem no display mostra o status exato do grupo alterado (ex.: "Sala acesa"/"Sala apagada", "Chuveiro ligado"/"Chuveiro desligado", "Forno ligado"/"Forno desligado"). **De dia** o botão é a única forma de mudar o estado — fica assim até o próximo toque, reinício ou troca de modo. **De noite** o botão também pausa o roteiro automático por 5s ("stand-by"); passado esse tempo sem novo toque, o roteiro retoma exatamente do ponto (do tempo) em que parou, já refletindo o estado alterado até o próximo evento da tabela corrigir aquele LED.

**Botão de reinício**: **de dia**, acende todos os 8 LEDs de novo. **De noite**, reinicia do zero (t=0) o roteiro noturno.

**Consumo estimado**: cada LED representa uma carga com potência real aproximada (W) — 50W para cada luz (jardim e cômodos), 2000W para o forno e 3000W para o chuveiro (cargas pesadas de verdade, para o "% do máximo" no display fazer sentido comparando uma lâmpada com um chuveiro/forno). O máximo (todos ligados) é uma constante calculada uma vez.

**Tensão do banco de baterias**: a maquete só tem uma bateria de ~12V no divisor de A1, mas para simular um banco off-grid real (várias baterias em série) o firmware multiplica a leitura por 10 antes de mostrar no display — então o valor exibido representa a tensão de um banco de ~120V, não a tensão física medida na protoboard.

**Display em tempo real** mostra: modo atual, o passo/ação atual do roteiro (ou o status do botão pressionado), consumo instantâneo estimado (W), tensão do banco de baterias simulado (V) e o % desse consumo em relação ao máximo (todos os LEDs ligados).
