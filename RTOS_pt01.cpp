#include <Arduino.h>
#include <ArduinoJson.h>

// Handle untuk Task
TaskHandle_t TaskSerialHandle = NULL;
TaskHandle_t TaskLogicHandle = NULL;

// Queue untuk mengirim data antar Task (opsional, tapi lebih aman)
QueueHandle_t dataQueue;

// Struktur data untuk menampung hasil parsing
struct MaggotData {
  float biopond[6];
  int harvest;
  float temp;
  float hum;
  float soil;
  int ammonia;
};

// --- TASK 1: Membaca Serial dari Mega ---
void TaskReadSerial(void *pvParameters) {
  Serial.begin(115200); // USB ke PC
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // RX2: 16, TX2: 17 (Hubungkan ke Mega)
  
  StaticJsonDocument<1024> doc;

  for (;;) {
    if (Serial2.available() > 0) {
      // Baca data sampai newline
      String input = Serial2.readStringUntil('\n');
      
      DeserializationError error = deserializeJson(doc, input);

      if (!error) {
        MaggotData newData;
        for(int i=0; i<6; i++) newData.biopond[i] = doc["biopond"][i];
        newData.harvest = doc["harvest"];
        newData.temp = doc["temp"];
        newData.hum = doc["hum"];
        newData.soil = doc["soil"];
        newData.ammonia = doc["ammonia"];

        // Kirim data ke Queue agar bisa dibaca Task lain
        xQueueOverwrite(dataQueue, &newData);
        
        Serial.println("Data Berhasil Diterima & Di-parse");
      } else {
        Serial.print("Gagal Parsing: ");
        Serial.println(error.c_str());
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Kasih nafas 10ms
  }
}

// --- TASK 2: Logika (Fuzzy / Upload Server) ---
void TaskProcessLogic(void *pvParameters) {
  MaggotData receivedData;

  for (;;) {
    // Cek apakah ada data baru di Queue
    if (xQueueReceive(dataQueue, &receivedData, portMAX_DELAY)) {
      
      // --- TEMPAT CODING FUZZY / MONITORING KAMU ---
      Serial.print("Processing Temp: ");
      Serial.println(receivedData.temp);
      
      // Contoh: Jika suhu > 30, nyalakan kipas (Logic kamu di sini)
    }
  }
}

void setup() {
  // Buat Queue (penampung data)
  dataQueue = xQueueCreate(1, sizeof(MaggotData));

  // Jalankan Task di Core yang berbeda (ESP32 punya Core 0 dan Core 1)
  
  xTaskCreatePinnedToCore(
    TaskReadSerial,    // Fungsi Task
    "ReadSerial",      // Nama Task
    5000,              // Stack Size
    NULL,              // Parameter
    2,                 // Prioritas (Tinggi)
    &TaskSerialHandle, // Handle
    0                  // Jalankan di Core 0
  );

  xTaskCreatePinnedToCore(
    TaskProcessLogic,
    "ProcessLogic",
    8000,
    NULL,
    1,                 // Prioritas (Lebih rendah dr serial)
    &TaskLogicHandle,
    1                  // Jalankan di Core 1
  );
}

void loop() {
  // Kosongkan loop karena sudah pakai RTOS
  vTaskDelete(NULL);
}