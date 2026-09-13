# Casa Inteligente com Autoconsumo Solar

Maquete de casa sustentável com Arduino Uno, controlador de carga solar e chuveiro elétrico simulado com termostato PWM.

## Compilar e enviar (PlatformIO)

Projeto configurado para PlatformIO (`platformio.ini`), alvo `uno` (Arduino Uno R3). A porta serial está fixada em `COM8` — ajuste em `platformio.ini` se a placa aparecer em outra porta.

```
pio run                # compila
pio run --target upload # compila e grava na placa
pio device monitor      # abre o monitor serial (9600 baud)
```

O sketch também pode ser aberto direto no Arduino IDE a partir de `src/ecohouse.ino`.

## Hardware

- Arduino Uno R3 + cabo USB
- Protoboard 400 furos + jumpers macho-macho
- Mini placa solar 5V/200mA + placa controladora de carga (bornes S+/S-, B+/B-, L+/L-)
- Suporte 4 pilhas AA + 4 pilhas recarregáveis Ni-MH (4,8V)
- 2x LDR 5516 + resistores 10kΩ (divisores de tensão)
- LEDs verde, amarelo, vermelho + resistores 220Ω

## Ligações

**Placa controladora solar**
- S+/S- → mini placa solar (+/-)
- B+/B- → suporte de pilhas (+/-)
- L- → GND do Arduino / trilha terra da protoboard
- L+ → trilha +5V da protoboard / pino 5V do Arduino

**Sensores (entradas analógicas)**
- LDR solar: +5V — LDR — A0 — resistor 10kΩ — GND
- LDR boiler (simula temperatura da água): +5V — LDR — A1 — resistor 10kΩ — GND

**Atuadores (saídas digitais)**
- LED verde (energia limpa ativa): D7 → resistor 220Ω → anodo; catodo → GND
- LED amarelo (climatização/cooler): D9 → resistor 220Ω → anodo; catodo → GND
- LED vermelho (chuveiro, PWM): D10 → resistor 220Ω → anodo; catodo → GND

## Lógica

- Sol > 60% → LED amarelo liga (climatização usa excesso de geração solar)
- Temp. boiler < 37°C → LED vermelho acende proporcionalmente (PWM) simulando o chuveiro completando o aquecimento
- Temp. boiler ≥ 37°C → chuveiro desligado (0% de energia da tomada)

Monitor Serial a 9600 baud mostra % de sol, temperatura do boiler e potência PWM do chuveiro a cada 500ms.
