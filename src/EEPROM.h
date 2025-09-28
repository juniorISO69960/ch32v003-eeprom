/******************************************************************************
 * EEPROM.h - Flash Storage Library for CH32V003J4M6
 *
 * Created by  Shakir Abdo
 *
 * Simple and reliable EEPROM emulation for the CH32V003 microcontroller.
 ******************************************************************************/

#ifndef EEPROM_H
#define EEPROM_H

#include <stddef.h>
#include <stdint.h>

#include "ch32v003fun.h"

// Status codes
#define EEPROM_OK 0
#define EEPROM_ERROR 1

// Address for storage - use a safe page
#define EEPROM_ADDRESS 0x08003C00

// Initialize EEPROM
void EEPROM_init(void);

// Format the flash page
uint8_t EEPROM_format(void);

// Variable operations
uint8_t EEPROM_saveVar(uint8_t id, uint16_t value);
uint16_t EEPROM_readVar(uint8_t id);
uint8_t EEPROM_saveVars(uint8_t *ids, uint16_t *values, uint8_t count);

// Check if variable exists
uint8_t EEPROM_varExists(uint8_t id);

#endif /* EEPROM_H */
