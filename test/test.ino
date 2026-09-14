// Teste minimo de hardware: pisca o LED interno (D13) e escreve "Arduino" no
// Nokia 5110. Serve pra isolar problema de fiacao/display do firmware
// principal (ecohouse.ino) — se isso funcionar, o problema esta na logica ou
// no wiring dos LEDs/botoes/PCF8574, nao no display em si.
//
// Como usar:
//   - Arduino IDE: abra este arquivo direto, instale as libs "Adafruit GFX
//     Library" e "Adafruit PCD8544 Nokia 5110 LCD library" (Sketch >
//     Include Library > Manage Libraries), compile e grave.
//   - PlatformIO: copie este arquivo para src/ecohouse.ino temporariamente
//     (faca backup do original antes) e rode `pio run --target upload`.
//
// Fiacao igual ao projeto principal (ver README.md):
//   CLK -> D8   DIN -> D9   DC -> D10   CE -> D11   RST -> D12
//   VCC -> 3.3V (NAO 5V)    GND -> GND

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 display = Adafruit_PCD8544(8, 9, 10, 11, 12);

const int PIN_LED = LED_BUILTIN; // D13

void setup() {
  pinMode(PIN_LED, OUTPUT);

  display.begin();
  display.setContrast(50);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(0, 16);
  display.print("Arduino");
  display.display();
}

void loop() {
  digitalWrite(PIN_LED, HIGH);
  delay(500);
  digitalWrite(PIN_LED, LOW);
  delay(500);
}
