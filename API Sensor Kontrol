#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "Test";
const char* password = "hahahaha";
//ENDPOINT URL untuk mengirim data sensor dan menerima data kontrol
const char* urlSensor = "http://10.106.170.67:8000/api/sensor"; 
const char* urlControl = "http://10.106.170.67:8000/api/control";

void setup() {
  Serial.begin(115200);
  
  // Serial2 untuk menerima dari Mega (RX2: Pin 16, TX2: Pin 17)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.begin(ssid, password);
  Serial.print("Koneksi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
}

void loop() {
  if (Serial2.available()) {
    String jsonPayload = Serial2.readStringUntil('\n'); 
    jsonPayload.trim(); 
    
    if (jsonPayload.startsWith("{") && jsonPayload.endsWith("}")) {
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        
        // --- 1. KIRIM DATA SENSOR (POST) ---
        http.begin(urlSensor);
        http.addHeader("Content-Type", "application/json");
        http.POST(jsonPayload);
        http.end();

        // --- 2. AMBIL DATA KONTROL (GET) ---
        http.begin(urlControl);
        int getCode = http.GET();
        if (getCode == 200) {
          String commandStr = http.getString();
          // Lempar perintah ke Mega via Serial2
          Serial2.println(commandStr); 
          Serial.println("Perintah Web dikirim ke Mega: " + commandStr);
        }
        http.end();
      }
    } else {
      while(Serial2.available()) Serial2.read(); 
    }
  }
}