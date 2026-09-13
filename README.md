# Casa Inteligente com Autoconsumo Solar

Maquete de casa sustentável com Arduino Uno, display Nokia 5110 (PCD8544, monocromático, sem touch), 5 grupos de LEDs representando cômodos e 6 botões de controle manual, simulando o consumo de energia ao longo de um ciclo dia/noite.

## Compilar e enviar (PlatformIO)

Projeto configurado para PlatformIO (`platformio.ini`), alvo `uno` (Arduino Uno R3). A porta está fixada em `COM8` — ajuste em `platformio.ini` se a placa aparecer em outra porta.

```
pio run                 # compila
pio run --target upload # compila e grava na placa
```

O sketch também pode ser aberto direto no Arduino IDE a partir de `src/ecohouse.ino`.

## Hardware

- Arduino Uno R3 + cabo USB
- Protoboard 400 furos + jumpers macho-macho
- Mini placa solar 5V/200mA + placa controladora de carga (bornes S+/S-, B+/B-, L+/L-)
- Suporte 4 pilhas AA + 4 pilhas recarregáveis Ni-MH (4,8V nominal)
- LDR 5516 + resistor 10kΩ (divisor de tensão, sensor dia/noite)
- Display Nokia 5110 (PCD8544, monocromático, sem touch)
- 8 LEDs (dos 15 disponíveis) + resistores 220Ω, agrupados em 5 pinos
- 6 botões grandes (5 de grupo + 1 de reinício), sem resistor externo (usa o pull-up interno do Arduino)
- Resistores 100kΩ + 10kΩ (divisor de tensão para medir a bateria em A1)

## Ligações

**Placa controladora solar**
- S+/S- → mini placa solar (+/-)
- B+/B- → suporte de pilhas (+/-)
- L- → GND do Arduino / trilha terra da protoboard
- L+ → trilha +5V da protoboard / pino 5V do Arduino

**Sensor (entrada analógica)**
- LDR solar (dia/noite): +5V — LDR — A0 — resistor 10kΩ — GND

**Tensão da bateria (entrada analógica)** — ⚠️ ainda não montado fisicamente, o firmware já espera por ele
- Divisor: B+/L+ (positivo da bateria) — resistor 100kΩ — nó — A1 — resistor 10kΩ — GND
- O nó entre os dois resistores é o que vai para A1. Sem esse divisor, a leitura de tensão no display não tem significado (pino flutuando).
- Por quê: o Arduino é alimentado pela própria bateria (via L+/L- do controlador), então medir com a referência padrão (5V/AVcc) só daria uma razão constante em relação a si mesma. O código usa a referência interna de 1,1V do ATmega328P para medir de verdade — daí o divisor precisar reduzir a tensão da bateria (até uns 5,8V) para bem abaixo de 1,1V antes de chegar em A1.

**Display Nokia 5110 (SPI)**
| Pino do display | Pino do Arduino |
|---|---|
| CLK  | D13 |
| DIN  | D11 |
| DC   | D5  |
| CE   | D4  |
| RST  | D3  |
| VCC  | 3,3V (não 5V) |
| GND  | GND |
| BL (luz de fundo) | 3,3V ou GND via resistor, conforme o módulo |

**LEDs por grupo** (cada LED: anodo → resistor 220Ω → pino; catodo → GND). Grupos com 2 LEDs ligam ambos em paralelo no mesmo pino.
| Grupo | Pino | LEDs |
|---|---|---|
| Jardim | D2 | Jardim 1 + Jardim 2 |
| Sala | D6 | Sala |
| Quarto | D7 | Quarto |
| Banheiro + Chuveiro | D8 | Banheiro + Chuveiro |
| Cozinha + Forno | D9 | Cozinha + Forno |

**Botões** (uma perna no pino, outra no GND — sem resistor, usa `INPUT_PULLUP`)
| Botão | Pino | Ação |
|---|---|---|
| Jardim | A2 | Liga/desliga o grupo Jardim |
| Sala | A3 | Liga/desliga o grupo Sala |
| Quarto | A4 | Liga/desliga o grupo Quarto |
| Banheiro + Chuveiro | A5 | Liga/desliga o grupo |
| Cozinha + Forno | D10 | Liga/desliga o grupo |
| Reinício | D12 | Reinicia do zero o ciclo atual (dia ou noite) |

D0/D1 (RX/TX) não são usados, para não atrapalhar a gravação por USB.

## Lógica

- **Início de cada ciclo (troca dia/noite ou botão de reinício)**: acende todos os LEDs por ~2,5s e guarda esse valor como consumo máximo (100% de referência) — é contra ele que a % em tempo real é calculada. Depois disso entra na sequência normal do modo.
- **Sensor de luz (A0) ≤ 300** → modo noturno: tour pela casa (jardim → sala → quarto → banho+chuveiro → cozinha+forno → sala → quarto), acendendo só o grupo "ocupado" da vez.
- **Sensor de luz (A0) > 300** → modo diurno: a cada 5s sorteia uma combinação aleatória de grupos ligados, simulando uso descuidado em pleno sol.
- **Qualquer botão de grupo**: alterna o estado daquele grupo na hora e pausa o avanço automático da sequência por 5s ("stand-by") — os outros grupos mantêm o estado atual. Passado esse tempo sem novo toque, a sequência automática continua a partir do estado já alterado (no modo diurno sorteia o próximo combo; no modo noturno segue o tour normalmente). A mensagem no display mostra o status exato do grupo alterado (ex.: "Sala acesa"/"Sala apagada", "Chuveiro ligado"/"Chuveiro desligado", "Forno ligado"/"Forno desligado").
- **Botão de reinício**: reinicia do zero o ciclo atual (dia ou noite), passando de novo pela abertura "todos ligados".
- **Display em tempo real** mostra: modo atual, última ação/status, consumo instantâneo estimado (mV), tensão da bateria (V) e o % desse consumo em relação ao máximo (todos os LEDs ligados).
