#include <Arduino.h>

#define TEST_PC 1 // macro, la metto così se devo escludere il codice a seguire, nel caso in cui collega la stm32, mi basta impostare questo valore a 0

static char line[64];
static size_t idx = 0;

#if TEST_PC
    #define U_IN Serial
    #define U_OUT Serial
#else                      // quindi se setto TEST_PC = 1 ho le periferiche usb, se invece setto 1 utilizzo la comunicazione UART
    #define U_IN Serial1
    #define U_OUT Serial1
#endif

void setup(){
    Serial.begin(115200);
    Serial1.begin(115200);

    #if TEST_PC 
        Serial.println(F("Mega in modalità TEST. Scrivi una riga e premi invio"));
    #else
        Serial1.println(F("Mega in modalità responder (TX e RX abilitati)"));
    #endif
}

void loop(){
    while(U_IN.available() > 0){
        char c = (char)U_IN.read();

        if(c == '\n'){
            line[idx] = 0;

            Serial.print(F("B RX: ")); // log su usb per vedere cos'è arrivato
            Serial.println(line);

            U_OUT.println(F("ACK_B"));
            //U_OUT.print(F("B TX: ACK_B"));

            idx = 0;
        }
        else if(idx < sizeof(line)-1) // evito overflow 
            line[idx++] = c;
    }
}


