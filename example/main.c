#include "EEPROM.h"
#include "ch32v003fun.h"
uint16_t test = 0;
int main() {
    SystemInit();

    // Initialize UART for debug output
    Delay_Ms(100);
    EEPROM_init();
    printf("EEPROM Demo\n");
    test = EEPROM_readVar(1);

    // Main loop
    while (1) {
        Delay_Ms(1000);
        printf("value: %d\n", test);
        test++;
        EEPROM_saveVar(1, test);
        Delay_Ms(5000);
    }
}
