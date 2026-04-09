#include <Arduino.h>
#include <ArduinoJson.h>
#include <Fuzzy.h>

// ==========================================
// 1. KONFIGURASI PIN & STRUKTUR DATA
// ==========================================
// Pin PWM Kipas 6 Biopond
const int pinPompa[6] = {25, 26, 27, 14, 12, 13}; 
// (BARU) Pin Relay Mist Maker 6 Biopond (Sesuaikan dengan wiring kamu)
const int pinMist[6]  = {32, 33, 2, 4, 5, 15};    

#define RX2_PIN 16
#define TX2_PIN 17

struct MaggotData {
  float biopond[6]; 
  int harvest;
  float temp;       
  float hum;
  float soil;
  int ammonia;
};

// Array untuk menyimpan "Kapan Mist Maker harus mati?"
unsigned long mistOffTime[6] = {0, 0, 0, 0, 0, 0};


// ==========================================
// 2. VARIABEL GLOBAL RTOS & FUZZY
// ==========================================
QueueHandle_t dataQueue;
TaskHandle_t TaskSerialHandle = NULL;
TaskHandle_t TaskLogicHandle = NULL;

Fuzzy *fuzzy = new Fuzzy();

// Variabel Fuzzy agar gampang diakses
FuzzySet *suhuDingin, *suhuNormal, *suhuPanas;
FuzzySet *tanahKering, *tanahNormal, *tanahBasah;
FuzzySet *kipasOff, *kipasLambat, *kipasKencang;
FuzzySet *mistOff, *mistSebentar, *mistLama;

// ==========================================
// 3. FUNGSI SETUP FUZZY (Dari MATLAB .fis)
// ==========================================
void setupFuzzy() {
  // --- INPUT 1: SUHU ---
  FuzzyInput *suhu = new FuzzyInput(1);
  suhuDingin = new FuzzySet(0, 0, 27, 27.5);
  suhuNormal = new FuzzySet(27, 28, 29, 30);
  suhuPanas  = new FuzzySet(29.5, 30, 36, 36);
  suhu->addFuzzySet(suhuDingin);
  suhu->addFuzzySet(suhuNormal);
  suhu->addFuzzySet(suhuPanas);
  fuzzy->addFuzzyInput(suhu);

  // --- INPUT 2: KELEMBAPAN TANAH ---
  FuzzyInput *kelembapanTanah = new FuzzyInput(2);
  tanahKering = new FuzzySet(0, 0, 60, 65);
  tanahNormal = new FuzzySet(60, 65, 75, 80);
  tanahBasah  = new FuzzySet(75, 80, 100, 100);
  kelembapanTanah->addFuzzySet(tanahKering);
  kelembapanTanah->addFuzzySet(tanahNormal);
  kelembapanTanah->addFuzzySet(tanahBasah);
  fuzzy->addFuzzyInput(kelembapanTanah);

  // --- OUTPUT 1: KIPAS (0 - 2000) ---
  FuzzyOutput *kipas = new FuzzyOutput(1);
  kipasOff     = new FuzzySet(0, 0, 500, 700);
  kipasLambat  = new FuzzySet(500, 800, 1400, 1700);
  kipasKencang = new FuzzySet(1500, 1700, 2000, 2000);
  kipas->addFuzzySet(kipasOff);
  kipas->addFuzzySet(kipasLambat);
  kipas->addFuzzySet(kipasKencang);
  fuzzy->addFuzzyOutput(kipas);

  // --- OUTPUT 2: MIST MAKER (0 - 15) ---
  FuzzyOutput *mistMaker = new FuzzyOutput(2);
  mistOff      = new FuzzySet(0, 0, 4, 6);
  mistSebentar = new FuzzySet(4, 6.5, 8.5, 11);
  mistLama     = new FuzzySet(9, 11, 15, 15);
  mistMaker->addFuzzySet(mistOff);
  mistMaker->addFuzzySet(mistSebentar);
  mistMaker->addFuzzySet(mistLama);
  fuzzy->addFuzzyOutput(mistMaker);

  // --- 9 ATURAN FUZZY (RULES) ---
  FuzzyRuleAntecedent *ifR1 = new FuzzyRuleAntecedent();
  ifR1->joinWithAND(suhuDingin, tanahKering);
  FuzzyRuleConsequent *thenR1 = new FuzzyRuleConsequent();
  thenR1->addOutput(kipasOff);
  thenR1->addOutput(mistSebentar);
  fuzzy->addFuzzyRule(new FuzzyRule(1, ifR1, thenR1));

  FuzzyRuleAntecedent *ifR2 = new FuzzyRuleAntecedent();
  ifR2->joinWithAND(suhuDingin, tanahNormal);
  FuzzyRuleConsequent *thenR2 = new FuzzyRuleConsequent();
  thenR2->addOutput(kipasOff);
  thenR2->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(2, ifR2, thenR2));

  FuzzyRuleAntecedent *ifR3 = new FuzzyRuleAntecedent();
  ifR3->joinWithAND(suhuDingin, tanahBasah);
  FuzzyRuleConsequent *thenR3 = new FuzzyRuleConsequent();
  thenR3->addOutput(kipasOff);
  thenR3->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(3, ifR3, thenR3));

  FuzzyRuleAntecedent *ifR4 = new FuzzyRuleAntecedent();
  ifR4->joinWithAND(suhuNormal, tanahKering);
  FuzzyRuleConsequent *thenR4 = new FuzzyRuleConsequent();
  thenR4->addOutput(kipasOff);
  thenR4->addOutput(mistLama);
  fuzzy->addFuzzyRule(new FuzzyRule(4, ifR4, thenR4));

  FuzzyRuleAntecedent *ifR5 = new FuzzyRuleAntecedent();
  ifR5->joinWithAND(suhuNormal, tanahNormal);
  FuzzyRuleConsequent *thenR5 = new FuzzyRuleConsequent();
  thenR5->addOutput(kipasOff);
  thenR5->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(5, ifR5, thenR5));

  FuzzyRuleAntecedent *ifR6 = new FuzzyRuleAntecedent();
  ifR6->joinWithAND(suhuNormal, tanahBasah);
  FuzzyRuleConsequent *thenR6 = new FuzzyRuleConsequent();
  thenR6->addOutput(kipasLambat);
  thenR6->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(6, ifR6, thenR6));

  FuzzyRuleAntecedent *ifR7 = new FuzzyRuleAntecedent();
  ifR7->joinWithAND(suhuPanas, tanahKering);
  FuzzyRuleConsequent *thenR7 = new FuzzyRuleConsequent();
  thenR7->addOutput(kipasKencang);
  thenR7->addOutput(mistLama);
  fuzzy->addFuzzyRule(new FuzzyRule(7, ifR7, thenR7));

  FuzzyRuleAntecedent *ifR8 = new FuzzyRuleAntecedent();
  ifR8->joinWithAND(suhuPanas, tanahNormal);
  FuzzyRuleConsequent *thenR8 = new FuzzyRuleConsequent();
  thenR8->addOutput(kipasKencang);
  thenR8->addOutput(mistSebentar);
  fuzzy->addFuzzyRule(new FuzzyRule(8, ifR8, thenR8));

  FuzzyRuleAntecedent *ifR9 = new FuzzyRuleAntecedent();
  ifR9->joinWithAND(suhuPanas, tanahBasah);
  FuzzyRuleConsequent *thenR9 = new FuzzyRuleConsequent();
  thenR9->addOutput(kipasKencang);
  thenR9->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(9, ifR9, thenR9));
}

// ==========================================
// 4. TASK RTOS: BACA SERIAL DARI MEGA
// ==========================================
void TaskReadSerial(void *pvParameters) {
  Serial2.begin(115200, SERIAL_8N1, RX2_PIN, TX2_PIN);
  StaticJsonDocument<1024> doc;

  for (;;) {
    if (Serial2.available() > 0) {
      String input = Serial2.readStringUntil('\n');
      DeserializationError error = deserializeJson(doc, input);

      if (!error) {
        MaggotData newData;
        for(int i = 0; i < 6; i++) {
          newData.biopond[i] = doc["biopond"][i]; // Format ini disesuaikan dengan kiriman Mega
        }
        newData.temp = doc["temp"];
        newData.ammonia = doc["ammonia"];
        
        xQueueOverwrite(dataQueue, &newData);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// 5. TASK RTOS: LOGIKA FUZZY & AKTUATOR
// ==========================================
void TaskProcessLogic(void *pvParameters) {
  setupFuzzy();
  MaggotData receivedData;

  for (;;) {
    // Timeout antrean diubah jadi 100ms (Sangat cepat). 
    // Ini agar core tidak terkunci dan tetap bisa mengecek timer Mist Maker di bawah.
    if (xQueueReceive(dataQueue, &receivedData, 100 / portTICK_PERIOD_MS)) {
      
      Serial.println("\n--- EVALUASI FUZZY 6 BIOPOND ---");
      Serial.printf("Suhu Universal: %.2f *C\n", receivedData.temp);

      for (int i = 0; i < 6; i++) {
        // --- 1. Evaluasi ---
        fuzzy->setInput(1, receivedData.temp);       
        fuzzy->setInput(2, receivedData.biopond[i]); 
        fuzzy->fuzzify();
        
        float outputKipas = fuzzy->defuzzify(1);     // Rentang 0 - 2000
        float outputMist = fuzzy->defuzzify(2);      // Rentang 0 - 15 (Detik)
        
        // --- 2. Eksekusi Kipas (PWM) ---
        int pwmKipas = map(outputKipas, 0, 2000, 0, 255);
        analogWrite(pinPompa[i], pwmKipas);

        // --- 3. Eksekusi Mist Maker (Non-Blocking) ---
        // Jika hasil fuzzy lebih dari 1 detik, nyalakan relay
        if (outputMist > 1.0) { 
          digitalWrite(pinMist[i], HIGH); // Nyalakan Mist Maker
          
          // Set alarm untuk dimatikan pada sekian detik ke depan
          // (millis() saat ini + durasi fuzzy dikonversi ke milidetik)
          mistOffTime[i] = millis() + ((unsigned long)(outputMist * 1000));
        }

        Serial.printf("Biopond %d | Lembap: %.1f%% | Kipas PWM: %d | Mist (Set Timer): %.1f dtk\n", 
                      i + 1, receivedData.biopond[i], pwmKipas, outputMist);
      }
      Serial.println("--------------------------------");
    }

    // --- 4. PANTAU TIMER MIST MAKER SECARA PARALEL ---
    // Bagian ini dieksekusi terus-menerus tanpa henti
    unsigned long currentMillis = millis();
    for (int i = 0; i < 6; i++) {
      // Jika alarm timer suatu biopond sudah lewat waktunya...
      if (mistOffTime[i] > 0 && currentMillis >= mistOffTime[i]) {
        digitalWrite(pinMist[i], LOW); // Matikan relay Mist Maker
        mistOffTime[i] = 0;            // Reset alarm
        
        Serial.printf("--> Mist Maker Biopond %d MATI.\n", i + 1);
      }
    }
  }
}

// ==========================================
// 6. FUNGSI WAJIB: SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  // Set pin Kipas dan Mist Maker
  for (int i = 0; i < 6; i++) {
    pinMode(pinPompa[i], OUTPUT);
    digitalWrite(pinPompa[i], LOW); 
    
    pinMode(pinMist[i], OUTPUT);     // (BARU) Setup Pin Mist Maker
    digitalWrite(pinMist[i], LOW); 
  }
}

void loop() {
  // Loop bawaan dihapus karena kita menggunakan FreeRTOS Task
  vTaskDelete(NULL);
}