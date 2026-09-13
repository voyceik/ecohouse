# Casa Inteligente com Autoconsumo Solar

Maquete de casa sustentável com Arduino Uno, display Nokia 5110 (PCD8544, monocromático, sem touch) e 8 LEDs representando cômodos, simulando o consumo de energia ao longo de um ciclo dia/noite.

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
- Suporte 4 pilhas AA + 4 pilhas recarregáveis Ni-MH (4,8V)
- LDR 5516 + resistor 10kΩ (divisor de tensão, sensor dia/noite)
- Display Nokia 5110 (PCD8544, monocromático, sem touch)
- 8 LEDs (um por cômodo) + resistores 220Ω

## Ligações

**Placa controladora solar**
- S+/S- → mini placa solar (+/-)
- B+/B- → suporte de pilhas (+/-)
- L- → GND do Arduino / trilha terra da protoboard
- L+ → trilha +5V da protoboard / pino 5V do Arduino

**Sensor (entrada analógica)**
- LDR solar (dia/noite): +5V — LDR — A0 — resistor 10kΩ — GND

**Display Nokia 5110 (SPI)**
| Pino do display | Pino do Arduino |
|---|---|
| CLK  | D13 |
| DIN  | D11 |
| DC   | D5  |
| CE   | D4  |
| RST  | D3  |
| VCC  | 3,3V |
| GND  | GND |
| BL (luz de fundo) | 3,3V ou GND via resistor, conforme o módulo |

**LEDs dos cômodos** (anodo → resistor 220Ω → pino; catodo → GND)
| LED | Pino |
|---|---|
| Jardim 1 | D2 |
| Jardim 2 | D6 |
| Sala | D7 |
| Cozinha | D8 |
| Quarto | D9 |
| Banheiro | D10 |
| Forno | D12 |
| Chuveiro | A1 (usado como digital) |

## Lógica

- **Sensor de luz (A0) > 300** → modo diurno: acende os LEDs em sequência (todos, só jardim, jardim+cômodos, todos de novo, e uma simulação de "esquecimento" acendendo cômodo por cômodo) para ilustrar o consumo em pleno sol.
- **Sensor de luz (A0) ≤ 300** → modo noturno: simula uma rotina real pela casa (jardim → sala → quarto → banho → cozinha → sala → quarto → dormir), acendendo só o LED do cômodo em uso e apagando o anterior — consumo consciente por bateria.
- O display mostra o modo atual, a ação em curso e o consumo instantâneo estimado (mV, baseado em tensões aproximadas por LED).
- Ao fim do ciclo noturno, o display mostra o **percentual de economia** comparando o consumo acumulado à noite (uso consciente) com o acumulado durante a simulação diurna (uso sem cuidado).
