#include <Arduino.h>


#define RXD2 16
#define TXD2 17

void setup() {
  // Serial Monitor untuk PC
  Serial.begin(115200);
  
  // Komunikasi Serial2 ke Mega
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  Serial.println("Tes Komunikasi ESP32 Dimulai. Menunggu Mega...");
}

void loop() {
  // Mendengarkan pesan dari Arduino Mega
  if (Serial2.available()) {
    // Membaca pesan yang masuk sampai menemukan Enter
    String pesanMasuk = Serial2.readStringUntil('\n');
    
    // Tampilkan di Serial Monitor PC
    Serial.print("Pesan masuk dari Mega: ");
    Serial.println(pesanMasuk);

    // Langsung balas ke Mega
    Serial2.println("PONG! ESP32 mendengarmu.");
  }
}