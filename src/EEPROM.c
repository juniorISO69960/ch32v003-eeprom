/******************************************************************************
 * EEPROM.c - Flash Storage Library for CH32V003J4M6
 *
 * Created by  Shakir Abdo
 ******************************************************************************/

#include "EEPROM.h"

#include <stddef.h>

// Flash keys for unlocking
#define FLASH_KEY1 ((uint32_t)0x45670123)
#define FLASH_KEY2 ((uint32_t)0xCDEF89AB)

// EEPROM identifiers
#define EEPROM_MARKER 0x5A5A

// Memory map:
// EEPROM_ADDRESS + 0: Marker (16-bit)
// EEPROM_ADDRESS + 2: Reserved (16-bit)
// EEPROM_ADDRESS + 4: Variable storage (triplets of 16-bit: ID, Value, CRC)

// Initialize EEPROM
void EEPROM_init(void) {
    // Nothing to initialize
}

// Simple CRC calculation
static uint16_t EEPROM_calcCRC(uint16_t id, uint16_t value) {
    return (uint16_t)(id ^ value);
}

// Wait for flash operations to complete
static uint8_t EEPROM_waitForLastOperation(void) {
    uint32_t timeout = 50000;

    while ((FLASH->STATR & FLASH_STATR_BSY) && timeout) {
        timeout--;
    }

    return (timeout > 0) ? EEPROM_OK : EEPROM_ERROR;
}

// Unlock flash for writing
static void EEPROM_unlockFlash(void) {
    if (FLASH->CTLR & FLASH_CTLR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

// Lock flash after writing
static void EEPROM_lockFlash(void) { FLASH->CTLR |= FLASH_CTLR_LOCK; }

// Erase a page of flash
uint8_t EEPROM_format(void) {
    uint8_t status;

    EEPROM_unlockFlash();

    // Wait for any ongoing operations
    status = EEPROM_waitForLastOperation();
    if (status != EEPROM_OK) {
        EEPROM_lockFlash();
        return status;
    }

    // Set page erase bit
    FLASH->CTLR |= FLASH_CTLR_PER;
    // Set the address to erase
    FLASH->ADDR = EEPROM_ADDRESS;
    // Start the erase operation
    FLASH->CTLR |= FLASH_CTLR_STRT;

    // Wait for completion
    status = EEPROM_waitForLastOperation();

    // Clear page erase bit
    FLASH->CTLR &= ~FLASH_CTLR_PER;

    // Check if erase worked
    if (*(volatile uint16_t*)EEPROM_ADDRESS != 0xFFFF) {
        EEPROM_lockFlash();
        return EEPROM_ERROR;
    }

    EEPROM_lockFlash();
    return status;
}

// Write a 16-bit value to flash
static uint8_t EEPROM_writeHalfWord(uint32_t address, uint16_t data) {
    uint8_t status;

    EEPROM_unlockFlash();

    // Wait for any ongoing operations
    status = EEPROM_waitForLastOperation();
    if (status != EEPROM_OK) {
        EEPROM_lockFlash();
        return status;
    }

    // Enable programming
    FLASH->CTLR |= FLASH_CTLR_PG;

    // Write the data
    *(volatile uint16_t*)address = data;

    // Wait for completion
    status = EEPROM_waitForLastOperation();

    // Disable programming
    FLASH->CTLR &= ~FLASH_CTLR_PG;

    // Verify the write
    if (*(volatile uint16_t*)address != data) {
        EEPROM_lockFlash();
        return EEPROM_ERROR;
    }

    EEPROM_lockFlash();
    return status;
}

// Check if EEPROM is initialized
static uint8_t EEPROM_isInitialized(void) {
    return (*(volatile uint16_t*)EEPROM_ADDRESS == EEPROM_MARKER);
}

// Find a variable by ID
static uint8_t EEPROM_findVar(uint8_t id, uint32_t* addr) {
    if (!EEPROM_isInitialized()) {
        return 0;
    }

    uint32_t currentAddr =
        EEPROM_ADDRESS + 4;  // Start after marker and reserved

    for (uint8_t i = 0; i < 10; i++) {  // Max 10 variables
        uint16_t entryId = *(volatile uint16_t*)currentAddr;

        // Check for end of data (empty slot)
        if (entryId == 0xFFFF) break;

        uint16_t entryValue = *(volatile uint16_t*)(currentAddr + 2);
        uint16_t entryCRC = *(volatile uint16_t*)(currentAddr + 4);

        // Check if this is our variable and CRC matches
        if ((entryId & 0xFF) == id &&
            entryCRC == EEPROM_calcCRC(entryId, entryValue)) {
            if (addr) *addr = currentAddr;
            return 1;
        }

        currentAddr += 6;  // Move to next entry (ID + Value + CRC)
    }

    return 0;  // Not found
}



// Save a variable
uint8_t EEPROM_saveVar(uint8_t id, uint16_t value) {
    uint8_t status;
    uint8_t varIds[10];  // Store up to 10 variables
    uint16_t varValues[10];
    uint8_t varCount = 0;

    // If not initialized, erase and initialize
    if (!EEPROM_isInitialized()) {
        status = EEPROM_format();
        if (status != EEPROM_OK) return status;

        // Write marker
        status = EEPROM_writeHalfWord(EEPROM_ADDRESS, EEPROM_MARKER);
        if (status != EEPROM_OK) return status;

        // Write reserved (just zeros)
        status = EEPROM_writeHalfWord(EEPROM_ADDRESS + 2, 0);
        if (status != EEPROM_OK) return status;
    } else {
        // Read all existing variables
        uint32_t currentAddr =
            EEPROM_ADDRESS + 4;  // Start after marker and reserved

        for (uint8_t i = 0; i < 10; i++) {  // Max 10 variables
            uint16_t entryId = *(volatile uint16_t*)currentAddr;

            // Check for end of data (empty slot)
            if (entryId == 0xFFFF) break;

            uint16_t entryValue = *(volatile uint16_t*)(currentAddr + 2);
            uint16_t entryCRC = *(volatile uint16_t*)(currentAddr + 4);

            // Skip the one we're updating
            if ((entryId & 0xFF) == id) {
                currentAddr += 6;  // Move to next entry
                continue;
            }

            // Check CRC
            if (entryCRC == EEPROM_calcCRC(entryId, entryValue)) {
                varIds[varCount] = entryId & 0xFF;
                varValues[varCount] = entryValue;
                varCount++;
            }

            currentAddr += 6;  // Move to next entry
        }
    }

    // Add our new variable
    varIds[varCount] = id;
    varValues[varCount] = value;
    varCount++;

    // Now write all variables to a new page
    status = EEPROM_format();
    if (status != EEPROM_OK) return status;

    // Write marker
    status = EEPROM_writeHalfWord(EEPROM_ADDRESS, EEPROM_MARKER);
    if (status != EEPROM_OK) return status;

    // Write reserved (just zeros)
    status = EEPROM_writeHalfWord(EEPROM_ADDRESS + 2, 0);
    if (status != EEPROM_OK) return status;

    // Write each variable
    uint32_t currentAddr = EEPROM_ADDRESS + 4;

    for (uint8_t i = 0; i < varCount; i++) {
        uint16_t varId = varIds[i];
        uint16_t varValue = varValues[i];
        uint16_t varCRC = EEPROM_calcCRC(varId, varValue);

        // Write ID
        status = EEPROM_writeHalfWord(currentAddr, varId);
        if (status != EEPROM_OK) return status;

        // Write value
        status = EEPROM_writeHalfWord(currentAddr + 2, varValue);
        if (status != EEPROM_OK) return status;

        // Write CRC
        status = EEPROM_writeHalfWord(currentAddr + 4, varCRC);
        if (status != EEPROM_OK) return status;

        currentAddr += 6;  // Move to next entry
    }

    return EEPROM_OK;
}

// Save multiple variables at once
uint8_t EEPROM_saveVars(uint8_t *ids, uint16_t *values, uint8_t count) {
    uint8_t status;
    uint8_t varIds[10];      // Store up to 10 variables
    uint16_t varValues[10];
    uint8_t varCount = 0;

    // If not initialized, erase and initialize
    if (!EEPROM_isInitialized()) {
        status = EEPROM_format();
        if (status != EEPROM_OK) return status;

        // Write marker
        status = EEPROM_writeHalfWord(EEPROM_ADDRESS, EEPROM_MARKER);
        if (status != EEPROM_OK) return status;

        // Write reserved (just zeros)
        status = EEPROM_writeHalfWord(EEPROM_ADDRESS + 2, 0);
        if (status != EEPROM_OK) return status;
    } else {
        // Read all existing variables
        uint32_t currentAddr = EEPROM_ADDRESS + 4;  // Start after marker

        for (uint8_t i = 0; i < 10; i++) {
            uint16_t entryId = *(volatile uint16_t*)currentAddr;

            // Check for end of data (empty slot)
            if (entryId == 0xFFFF) break;

            uint16_t entryValue = *(volatile uint16_t*)(currentAddr + 2);
            uint16_t entryCRC   = *(volatile uint16_t*)(currentAddr + 4);

            // Check CRC
            if (entryCRC == EEPROM_calcCRC(entryId, entryValue)) {
                uint8_t baseId = entryId & 0xFF;
                // Skip if this ID will be updated by our new list
                uint8_t skip = 0;
                for (uint8_t j = 0; j < count; j++) {
                    if (ids[j] == baseId) {
                        skip = 1;
                        break;
                    }
                }
                if (!skip) {
                    varIds[varCount] = baseId;
                    varValues[varCount] = entryValue;
                    varCount++;
                }
            }
            currentAddr += 6;  // Next entry
        }
    }

    // Append all new variables (overwriting any previous ones)
    for (uint8_t j = 0; j < count; j++) {
        if (varCount >= 10) break; // prevent overflow
        varIds[varCount] = ids[j];
        varValues[varCount] = values[j];
        varCount++;
    }

    // Erase page and rewrite everything
    status = EEPROM_format();
    if (status != EEPROM_OK) return status;

    // Write marker
    status = EEPROM_writeHalfWord(EEPROM_ADDRESS, EEPROM_MARKER);
    if (status != EEPROM_OK) return status;

    // Write reserved
    status = EEPROM_writeHalfWord(EEPROM_ADDRESS + 2, 0);
    if (status != EEPROM_OK) return status;

    // Write each variable
    uint32_t currentAddr = EEPROM_ADDRESS + 4;

    for (uint8_t i = 0; i < varCount; i++) {
        uint16_t varId    = varIds[i];
        uint16_t varValue = varValues[i];
        uint16_t varCRC   = EEPROM_calcCRC(varId, varValue);

        // Write ID
        status = EEPROM_writeHalfWord(currentAddr, varId);
        if (status != EEPROM_OK) return status;

        // Write value
        status = EEPROM_writeHalfWord(currentAddr + 2, varValue);
        if (status != EEPROM_OK) return status;

        // Write CRC
        status = EEPROM_writeHalfWord(currentAddr + 4, varCRC);
        if (status != EEPROM_OK) return status;

        currentAddr += 6;
    }

    return EEPROM_OK;
}


// Read a variable by ID
uint16_t EEPROM_readVar(uint8_t id) {
    uint32_t addr;

    if (EEPROM_findVar(id, &addr)) {
        return *(volatile uint16_t*)(addr + 2);
    }

    return 0xFFFF;  // Not found/invalid
}

// Check if variable exists
uint8_t EEPROM_varExists(uint8_t id) { return EEPROM_findVar(id, NULL); }
