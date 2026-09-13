# Protótipo Wokwi

`diagram.json` para simular a maquete no [wokwi.com](https://wokwi.com/projects/new/arduino-uno) antes de montar de verdade. Já reflete o hardware atual: 8 LEDs individuais via PCF8574 (I2C) e o roteiro fixo dia/noite do firmware.

## Como usar

1. Crie um novo projeto Arduino Uno em wokwi.com.
2. Copie o conteúdo de [`libraries.txt`](libraries.txt) para a aba **libraries.txt** do Wokwi (resolve o erro `Adafruit_PCD8544.h: No such file or directory`).
3. Copie o conteúdo de [`../src/ecohouse.ino`](../src/ecohouse.ino) para a aba **sketch.ino**. Esse arquivo não é duplicado aqui de propósito — sempre use a versão de `src/`, que é a que vai para a placa de verdade.
4. Abra a aba **diagram.json** no Wokwi e cole o conteúdo deste [`diagram.json`](diagram.json).
5. **Crie um "Custom Chip" para o PCF8574** — o Wokwi não tem essa peça pronta:
   - Clique no botão azul "+" no editor do diagrama e escolha "Custom Chip" (linguagem C), nomeie como `pcf8574`.
   - Isso cria dois arquivos no projeto: `pcf8574.chip.json` e `pcf8574.chip.c`. Substitua o conteúdo de cada um pelo deste repositório: [`pcf8574.chip.json`](pcf8574.chip.json) e [`pcf8574.chip.c`](pcf8574.chip.c).
   - O Wokwi compila o C automaticamente ao rodar a simulação (sem precisar de nenhuma ferramenta local).

## O que está simulado

- **8 LEDs individuais** (2 verdes = jardim, 4 amarelos = cômodos, 2 vermelhos = forno/chuveiro) acionados por um **PCF8574** simulado, ligado por I2C (A4=SDA, A5=SCL) — o mesmo esquema da maquete real.
- Display Nokia 5110 (SPI: D13/D11/D5/D4/D3, VCC em 3,3V).
- Sensor de luz (LDR) em A0 — arraste o slider de iluminância do LDR na simulação para alternar dia/noite.
- Divisor de tensão da bateria (100kΩ/10kΩ) em A1.
- 6 botões: Jardim (D2), Sala (D6), Quarto (D7), Banheiro+Chuveiro (D8), Cozinha+Forno (D9), Reinício (D10).

Como o `sketch.ino` é o firmware real sem nenhuma adaptação, o roteiro fixo dia/noite (ver `README.md` do projeto) roda idêntico ao que vai rodar na placa de verdade — inclusive a simulação de chuveiro/forno com atraso de 3s/10s/2s.

## Diferenças em relação ao circuito real

- **O chip PCF8574 é um "Custom Chip" simplificado**, escrito à mão para esta simulação (não existe uma peça nativa no Wokwi). Ele só implementa a parte usada pelo firmware: recebe o byte escrito por I2C e espelha cada bit em P0-P7 como saída digital. O PCF8574 de verdade é "quasi-bidirecional" (dá pra usar os mesmos pinos como entrada, puxando pra baixo externamente) — isso não é modelado aqui porque o firmware nunca lê os LEDs de volta.
- **O divisor da bateria está ligado ao 5V do Arduino**, não a uma bateria de verdade (o Wokwi não tem um pack de pilhas com tensão variável) — então a leitura de tensão no display vai mostrar sempre algo perto do valor "cheio", só serve pra validar a conta do divisor (100k/10k, referência interna 1,1V), não pra simular a bateria descarregando.
- **Nomes dos pinos do LDR**: usei os nomes de pino padrão do `wokwi-ldr` (`1`/`2`). Se o Wokwi reclamar de algum fio solto nesse componente, é só arrastar de novo no editor visual — o que importa é a topologia 5V→LDR→(nó)→10kΩ→GND com o nó em A0.
- Todos os GNDs novos apontam para `uno:GND.1`; o Wokwi permite vários fios no mesmo pino de GND sem problema.
