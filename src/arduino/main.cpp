#include <Arduino.h>

static char line[64]; //imposto un buffer di 64 byte.
static size_t idx = 0; // inizializzo l'indice.

void setup() { // funzione che arduino chiama una sola volta all'avvio/reset, prima di entrare nel ciclo infinito del void loop.
  Serial.begin(115200);  // inizializzo la porta usb (comunicazione via usb).
  Serial1.begin(115200);  // inizializzo la prta uart (pin 18(TX) e 19(RX)).
  Serial.println(F("Mega pronto, STM32 su serial 1 (18 e 19)"));
}

void loop() {
  while(Serial1.available() > 0)  {
    char c = (char)Serial1.read(); // faccio un cast perchè serial1 torna di default degli interi (il valore del byte letto).

    if(c == '\n' || c == '\r'){
      line[idx] = 0;
      Serial.print(F(" RX: "));
      Serial.println(line);
      Serial1.println(F("ACK_B"));
      Serial.print(F(" TX: ACK_B")); //log locale della trasmissione
      idx = 0;
    }
    else if(idx < sizeof(line)-1) // per evitare l'overflow
      line[idx++] = c;
  }
  //while (Serial.available()) Serial1.write(Serial.read()); // serve se voglio scrivere manualmente dei messaggi 

}
