// Modelo simplificado de um PCF8574 (expansor de I/O I2C) para simulação no
// Wokwi. O Wokwi não tem uma peça nativa de PCF8574, então este chip
// customizado escuta o endereço 0x20 (A0/A1/A2 aterrados) e espelha cada bit
// do último byte escrito diretamente nos pinos P0-P7. Simplificação: o
// PCF8574 real é "quasi-bidirecional" (leitura possível puxando o pino para
// baixo externamente); aqui os pinos P0-P7 são tratados só como saída, o
// suficiente para acionar os 8 LEDs da maquete.
#include "wokwi-api.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  pin_t pins[8];
} chip_state_t;

static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  return true; // ACK: só existe este dispositivo no endereço configurado
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  for (int i = 0; i < 8; i++) {
    pin_write(chip->pins[i], (data & (1 << i)) ? HIGH : LOW);
  }
  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  uint8_t result = 0;
  for (int i = 0; i < 8; i++) {
    if (pin_read(chip->pins[i]) == HIGH) result |= (1 << i);
  }
  return result;
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  const char *nomes[8] = { "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7" };
  for (int i = 0; i < 8; i++) {
    chip->pins[i] = pin_init(nomes[i], OUTPUT_HIGH); // PCF8574 real liga com todos em HIGH
  }

  pin_t scl = pin_init("SCL", INPUT_PULLUP);
  pin_t sda = pin_init("SDA", INPUT_PULLUP);

  const i2c_config_t i2c_config = {
    .user_data = chip,
    .address = 0x20, // A0=A1=A2=GND
    .scl = scl,
    .sda = sda,
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
  };
  i2c_init(&i2c_config);
}
