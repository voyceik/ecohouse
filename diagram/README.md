# Protótipo Wokwi

`diagram.json` para simular a maquete no [wokwi.com](https://wokwi.com/projects/new/arduino-uno) antes de montar de verdade.

## Como usar

1. Crie um novo projeto Arduino Uno em wokwi.com.
2. Copie o conteúdo de [`libraries.txt`](libraries.txt) para a aba **libraries.txt** do Wokwi (resolve o erro `Adafruit_PCD8544.h: No such file or directory`).
3. Copie o conteúdo de [`../src/ecohouse.ino`](../src/ecohouse.ino) para a aba **sketch.ino**. Esse arquivo não é duplicado aqui de propósito — sempre use a versão de `src/`, que é a que vai para a placa de verdade.
4. Abra a aba **diagram.json** no Wokwi e cole o conteúdo deste [`diagram.json`](diagram.json).

## O que está simulado

- 5 grupos de LEDs (D2, D6, D7, D8, D9) com resistor de 220Ω cada.
- Display Nokia 5110 (SPI: D13/D11/D5/D4/D3, VCC em 3,3V).
- Sensor de luz (LDR) em A0 — arraste o slider de iluminância do LDR na simulação para alternar dia/noite.
- Divisor de tensão da bateria (100kΩ/10kΩ) em A1.
- 6 botões (A2, A3, A4, A5, D10, D12).

## Diferenças em relação ao circuito real

- **Cada grupo tem só 1 LED no Wokwi**, não 2 (o `wokwi-led` não modela dois LEDs físicos em paralelo). Suficiente para testar a lógica; a maquete real usa 2 LEDs em paralelo nos grupos Jardim, Banheiro+Chuveiro e Cozinha+Forno.
- **O divisor da bateria está ligado ao 5V do Arduino**, não a uma bateria de verdade (o Wokwi não tem um pack de pilhas com tensão variável) — então a leitura de tensão no display vai mostrar sempre algo perto do valor "cheio", só serve pra validar a conta do divisor (100k/10k, referência interna 1,1V), não pra simular a bateria descarregando.
- **Nomes dos pinos do LDR**: usei os nomes de pino padrão do `wokwi-ldr` (`1`/`2`). Se o Wokwi reclamar de algum fio solto nesse componente, é só arrastar de novo no editor visual — o que importa é a topologia 5V→LDR→(nó)→10kΩ→GND com o nó em A0.
- Todos os GNDs novos apontam para `uno:GND.1`; o Wokwi permite vários fios no mesmo pino de GND sem problema.
