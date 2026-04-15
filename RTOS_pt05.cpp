#include <Arduino.h>
#include <ArduinoJson.h>
#include <Fuzzy.h>

// ==========================================
// 1. KONFIGURASI PIN & STRUKTUR DATA
// ==========================================
const int pinKipas[6] = {25, 26, 27, 14, 12, 13}; 
const int pinMist[6]  = {32, 33, 2, 4, 5, 15};    
#define RX2_PIN 16
#define TX2_PIN 17

struct MaggotData {
  float soil[6];    
  float temp;       
};

// ==========================================
// 2. VARIABEL GLOBAL
// ==========================================
unsigned long mistOffTime[6] = {0, 0, 0, 0, 0, 0};
QueueHandle_t dataQueue;
Fuzzy *fuzzy = new Fuzzy();

// Deklarasi Himpunan Fuzzy agar terbaca oleh Rules
FuzzySet *suhuDingin, *suhuNormal, *suhuPanas;
FuzzySet *tanahKering, *tanahNormal, *tanahBasah;
FuzzySet *kipasOff, *kipasLambat, *kipasKencang;
FuzzySet *mistOff, *mistSebentar, *mistLama;

// ==========================================
// 3. SETUP FUZZY (Mamdani .fis)
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
  FuzzyInput *tanah = new FuzzyInput(2);
  tanahKering = new FuzzySet(0, 0, 60, 65);
  tanahNormal = new FuzzySet(60, 65, 75, 80);
  tanahBasah  = new FuzzySet(75, 80, 100, 100);
  tanah->addFuzzySet(tanahKering);
  tanah->addFuzzySet(tanahNormal);
  tanah->addFuzzySet(tanahBasah);
  fuzzy->addFuzzyInput(tanah);

  // --- OUTPUT 1: KIPAS ---
  FuzzyOutput *kipas = new FuzzyOutput(1);
  kipasOff     = new FuzzySet(0, 0, 500, 700);
  kipasLambat  = new FuzzySet(500, 800, 1400, 1700);
  kipasKencang = new FuzzySet(1500, 1700, 2000, 2000);
  kipas->addFuzzySet(kipasOff);
  kipas->addFuzzySet(kipasLambat);
  kipas->addFuzzySet(kipasKencang);
  fuzzy->addFuzzyOutput(kipas);

  // --- OUTPUT 2: MIST MAKER ---
  FuzzyOutput *mist = new FuzzyOutput(2);
  mistOff      = new FuzzySet(0, 0, 4, 6);
  mistSebentar = new FuzzySet(4, 6.5, 8.5, 11);
  mistLama     = new FuzzySet(9, 11, 15, 15);
  mist->addFuzzySet(mistOff);
  mist->addFuzzySet(mistSebentar);
  mist->addFuzzySet(mistLama);
  fuzzy->addFuzzyOutput(mist);

  // --- 9 ATURAN FUZZY (RULES) ---
  // R1: 1 1, 1 2
  FuzzyRuleAntecedent *ifR1 = new FuzzyRuleAntecedent();
  ifR1->joinWithAND(suhuDingin, tanahKering);
  FuzzyRuleConsequent *thenR1 = new FuzzyRuleConsequent();
  thenR1->addOutput(kipasOff); thenR1->addOutput(mistSebentar);
  fuzzy->addFuzzyRule(new FuzzyRule(1, ifR1, thenR1));

  // R2: 1 2, 1 1
  FuzzyRuleAntecedent *ifR2 = new FuzzyRuleAntecedent();
  ifR2->joinWithAND(suhuDingin, tanahNormal);
  FuzzyRuleConsequent *thenR2 = new FuzzyRuleConsequent();
  thenR2->addOutput(kipasOff); thenR2->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(2, ifR2, thenR2));

  // R3: 1 3, 1 1
  FuzzyRuleAntecedent *ifR3 = new FuzzyRuleAntecedent();
  ifR3->joinWithAND(suhuDingin, tanahBasah);
  FuzzyRuleConsequent *thenR3 = new FuzzyRuleConsequent();
  thenR3->addOutput(kipasOff); thenR3->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(3, ifR3, thenR3));

  // R4: 2 1, 1 3
  FuzzyRuleAntecedent *ifR4 = new FuzzyRuleAntecedent();
  ifR4->joinWithAND(suhuNormal, tanahKering);
  FuzzyRuleConsequent *thenR4 = new FuzzyRuleConsequent();
  thenR4->addOutput(kipasOff); thenR4->addOutput(mistLama);
  fuzzy->addFuzzyRule(new FuzzyRule(4, ifR4, thenR4));

  // R5: 2 2, 1 1
  FuzzyRuleAntecedent *ifR5 = new FuzzyRuleAntecedent();
  ifR5->joinWithAND(suhuNormal, tanahNormal);
  FuzzyRuleConsequent *thenR5 = new FuzzyRuleConsequent();
  thenR5->addOutput(kipasOff); thenR5->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(5, ifR5, thenR5));

  // R6: 2 3, 2 1
  FuzzyRuleAntecedent *ifR6 = new FuzzyRuleAntecedent();
  ifR6->joinWithAND(suhuNormal, tanahBasah);
  FuzzyRuleConsequent *thenR6 = new FuzzyRuleConsequent();
  thenR6->addOutput(kipasLambat); thenR6->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(6, ifR6, thenR6));

  // R7: 3 1, 3 3
  FuzzyRuleAntecedent *ifR7 = new FuzzyRuleAntecedent();
  ifR7->joinWithAND(suhuPanas, tanahKering);
  FuzzyRuleConsequent *thenR7 = new FuzzyRuleConsequent();
  thenR7->addOutput(kipasKencang); thenR7->addOutput(mistLama);
  fuzzy->addFuzzyRule(new FuzzyRule(7, ifR7, thenR7));

  // R8: 3 2, 3 2
  FuzzyRuleAntecedent *ifR8 = new FuzzyRuleAntecedent();
  ifR8->joinWithAND(suhuPanas, tanahNormal);
  FuzzyRuleConsequent *thenR8 = new FuzzyRuleConsequent();
  thenR8->addOutput(kipasKencang); thenR8->addOutput(mistSebentar);
  fuzzy->addFuzzyRule(new FuzzyRule(8, ifR8, thenR8));

  // R9: 3 3, 3 1
  FuzzyRuleAntecedent *ifR9 = new FuzzyRuleAntecedent();
  ifR9->joinWithAND(suhuPanas, tanahBasah);
  FuzzyRuleConsequent *thenR9 = new FuzzyRuleConsequent();
  thenR9->addOutput(kipasKencang); thenR9->addOutput(mistOff);
  fuzzy->addFuzzyRule(new FuzzyRule(9, ifR9, thenR9));
}

// ==========================================
// 4. TASK BACA SERIAL
// ==========================================
void TaskReadSerial(void *pvParameters) {
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
  
  // Menggunakan JsonDocument (Perbaikan untuk ArduinoJson v7)
  JsonDocument doc; 

  for (;;) {
    if (Serial2.available() > 0) {
      String input = Serial2.readStringUntil('\n');
      
      DeserializationError error = deserializeJson(doc, input);
      if (!error) {
        MaggotData newData;
        newData.temp = doc["temp"];
        for(int i = 0; i < 6; i++) {
          newData.soil[i] = doc["soil"][i];
        }
        xQueueOverwrite(dataQueue, &newData);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ==========================================
// 5. TASK LOGIKA & AKTUATOR
// ==========================================
void TaskProcessLogic(void *pvParameters) {
  setupFuzzy();
  MaggotData data;

  for (;;) {
    if (xQueueReceive(dataQueue, &data, 100 / portTICK_PERIOD_MS)) {
      Serial.println("\n--- EVALUASI FUZZY ---");
      Serial.printf("Suhu: %.2f *C\n", data.temp);

      for (int i = 0; i < 6; i++) {
        fuzzy->setInput(1, data.temp);
        fuzzy->setInput(2, data.soil[i]);
        fuzzy->fuzzify();
        
        float outKipas = fuzzy->defuzzify(1);
        float outMist = fuzzy->defuzzify(2);

        // Eksekusi Kipas PWM
        int pwmKipas = map(outKipas, 0, 2000, 0, 255);
        analogWrite(pinKipas[i], pwmKipas);

        // Eksekusi Mist Maker
        if (outMist > 1.0) {
          digitalWrite(pinMist[i], HIGH);
          mistOffTime[i] = millis() + (unsigned long)(outMist * 1000);
        }

        Serial.printf("Bio %d | Lmbp: %.1f%% | Fan: %d | Mist: %.1fs\n", 
                      i + 1, data.soil[i], pwmKipas, outMist);
      }
    }

    // Pantau Timer Mist Maker
    unsigned long currentMillis = millis();
    for (int i = 0; i < 6; i++) {
      if (mistOffTime[i] > 0 && currentMillis >= mistOffTime[i]) {
        digitalWrite(pinMist[i], LOW);
        mistOffTime[i] = 0;
        Serial.printf("--> Mist Maker %d MATI\n", i + 1);
      }
    }
  }
}

// ==========================================
// 6. SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < 6; i++) {
    pinMode(pinKipas[i], OUTPUT);
    digitalWrite(pinKipas[i], LOW); 
    
    pinMode(pinMist[i], OUTPUT);
    digitalWrite(pinMist[i], LOW); 
  }

  dataQueue = xQueueCreate(1, sizeof(MaggotData));

  xTaskCreatePinnedToCore(TaskReadSerial, "Read", 5000, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskProcessLogic, "Logic", 10000, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}