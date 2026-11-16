#define BLYNK_TEMPLATE_ID   "TMPL6z2uH3wrp"
#define BLYNK_TEMPLATE_NAME "IOT"
#define BLYNK_AUTH_TOKEN    "THjb1RpmtbOApx-uoDm9tQB8xYZWP0hp"

#define BLYNK_PRINT Serial

// Aktifkan Pengujian Model AI (Set 1 untuk aktif, 0 untuk nonaktif)
#define TEST_MODE 1

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// 1. Inklusi Header Model Machine Learning (3 Kelas)
#include "model_3kelas.h"

#define DHTPIN 4
#define DHTTYPE DHT22

#define PIN_SOIL 34
#define PIN_BUTTON 18

// ============================================================================
// KALIBRASI SOIL MOISTURE TERKALIBRASI
// ============================================================================
const int RAW_DRY = 2520;
const int RAW_WET = 900;

float calibratePercent(float value) {
  const float measured[] = {0, 28, 45, 50, 73, 92, 98, 100};
  const float target[]   = {0, 25, 45, 50, 70, 88.68f, 93.40f, 100};
  value = constrain(value, 0.0f, 100.0f);

  for (int i = 0; i < 7; ++i) {
    if (value <= measured[i + 1]) {
      const float fraction = (value - measured[i]) /
                             (measured[i + 1] - measured[i]);
      return constrain(target[i] + fraction *
                       (target[i + 1] - target[i]), 0.0f, 100.0f);
    }
  }
  return 100.0f;
}

float hitungPersentaseTanahTerkalibrasi(int raw) {
  if (RAW_DRY == RAW_WET) return 0.0f;

  const int RAW_MID = RAW_DRY - (56 * (RAW_DRY - RAW_WET) / 100);
  float moisture;
  if (raw >= RAW_MID) {
    moisture = 50.0f * (RAW_DRY - raw) / (float)(RAW_DRY - RAW_MID);
  } else {
    moisture = 50.0f + 50.0f * (RAW_MID - raw) /
               (float)(RAW_MID - RAW_WET);
  }
  moisture = constrain(moisture, 0.0f, 100.0f);
  return calibratePercent(moisture);
}

LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
WiFiManager wifiManager;
Eloquent::ML::Port::PenyiramanModel3Kelas clf;

int currentScreen = 0;
const int TOTAL_SCREENS = 4;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

float suhu = 0.0;
float kelembapanUdara = 0.0;
float persentaseTanah = 0.0f;

String statusRekomendasi = "Tdk prlu siram";
String warnaRekomendasi  = "#2E7D32"; 

// --- VARIABEL PELACAK STATUS WIFI & RECONNECT ---
bool wasConnected = false;
String lastSSID = "";
unsigned long lastReconnectAttempt = 0; 

// ============================================================================
// 2. DEFINISI STRUKTUR & FUNGSI UJI MODEL
// ============================================================================
struct Kasus { float tanah; float suhu; int harapan; };
const Kasus uji[] = {
  {20,35,2},{20,25,1},{39,32.5,2},{39,31.5,1},{50,33,1},{50,25,0},
  {59,30.5,1},{59,29.5,0},{61,40,0},{80,25,0},{0,40,2},{100,20,0},
  {41,29.5,0},{40,29.5,0},{60,30.5,1}
};

void jalankanUjiModel() {
  int n = sizeof(uji) / sizeof(uji[0]), lulus = 0;
  for (int i = 0; i < n; i++) {
    float x[2] = { uji[i].tanah, uji[i].suhu };
    int h = clf.predict(x);
    bool ok = (h == uji[i].harapan);
    if (ok) lulus++;
  }
}

// ============================================================================
// 3. FUNGSI SENSOR & LOGIKA UTAMA
// ============================================================================
void bacaSemuaSensor() {
  kelembapanUdara = dht.readHumidity();
  suhu = dht.readTemperature();

  uint32_t totalSoil = 0;
  for (int i = 0; i < 16; ++i) {
    totalSoil += analogRead(PIN_SOIL);
    delay(2);
  }
  const int rawSoil = totalSoil / 16;
  persentaseTanah = hitungPersentaseTanahTerkalibrasi(rawSoil);

  if (!isnan(suhu)) {
    float x_input[2] = { persentaseTanah, suhu };
    int hasilPrediksi = clf.predict(x_input);

    if (hasilPrediksi == 2) {
      statusRekomendasi = "Wajib Siram!";
      warnaRekomendasi  = "#D32F2F"; 
    } else if (hasilPrediksi == 1) {
      statusRekomendasi = "Siram Ringan";
      warnaRekomendasi  = "#F9A825"; 
    } else {
      statusRekomendasi = "Tdk prlu siram";
      warnaRekomendasi  = "#2E7D32"; 
    }
  }
}

void kirimDataKeBlynkDanUpdateLCD() {
  bacaSemuaSensor();

  if (Blynk.connected() && WiFi.status() == WL_CONNECTED) {
    if (!isnan(suhu)) Blynk.virtualWrite(V0, suhu);
    if (!isnan(kelembapanUdara)) Blynk.virtualWrite(V1, kelembapanUdara);
    Blynk.virtualWrite(V2, persentaseTanah);
    Blynk.setProperty(V4, "color", warnaRekomendasi);
    Blynk.virtualWrite(V4, statusRekomendasi);
  }

  tampilkanLayar();
}

void tampilkanLayar() {
  char baris1[17];
  char baris2[17];

  switch (currentScreen) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Rekomendasi AI :");
      lcd.setCursor(0, 1);
      snprintf(baris2, sizeof(baris2), "> %-14s", statusRekomendasi.c_str());
      lcd.print(baris2);
      break;

    case 1:
      lcd.setCursor(0, 0);
      if (isnan(suhu)) lcd.print("Suhu  : Error   ");
      else { snprintf(baris1, sizeof(baris1), "Suhu  : %4.1f C ", suhu); lcd.print(baris1); }
      
      lcd.setCursor(0, 1);
      if (isnan(kelembapanUdara)) lcd.print("Lembap: Error   ");
      else { snprintf(baris2, sizeof(baris2), "Lembap: %4.1f %% ", kelembapanUdara); lcd.print(baris2); }
      break;

    case 2:
      lcd.setCursor(0, 0);
      snprintf(baris1, sizeof(baris1), "Tanah:%5.1f%%    ", persentaseTanah);
      lcd.print(baris1);
      lcd.setCursor(0, 1);
      if (persentaseTanah < 40) lcd.print("Status: Kering  ");
      else if (persentaseTanah <= 70) lcd.print("Status: Optimal ");
      else lcd.print("Status: Basah   ");
      break;

    case 3:
      lcd.setCursor(0, 0);
      if (Blynk.connected() && WiFi.status() == WL_CONNECTED) {
        lcd.print("Blynk: Connected");
      } else {
        lcd.print("Blynk: Offline  ");
      }
      
      lcd.setCursor(0, 1);
      if (WiFi.status() == WL_CONNECTED) {
        snprintf(baris2, sizeof(baris2), "WiFi :%-10s", lastSSID.c_str());
      } else {
        if (lastSSID != "") {
          snprintf(baris2, sizeof(baris2), "X %-14s", lastSSID.c_str());
        } else {
          snprintf(baris2, sizeof(baris2), "WiFi :Setup AP  ");
        }
      }
      lcd.print(baris2);
      break;
  }
}

// ============================================================================
// 4. SETUP UTAMA
// ============================================================================
void setup() {
  Serial.begin(115200);

  #if TEST_MODE
    jalankanUjiModel();
  #endif

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_SOIL, INPUT);
  analogSetPinAttenuation(PIN_SOIL, ADC_11db);
  analogReadResolution(12);

  Wire.begin(21, 23);
  lcd.init();
  lcd.backlight();
  dht.begin();

  lcd.setCursor(0, 0);
  lcd.print("Memulai Sistem..");
  delay(1000);
  
  // Matikan auto-reconnect bawaan agar tidak merusak sinyal AP
  WiFi.setAutoReconnect(false); 

  wifiManager.setConfigPortalBlocking(false); 
  wifiManager.autoConnect("SmartPlant_Setup");
  if (lastSSID == "") lastSSID = wifiManager.getWiFiSSID();
  Blynk.config(BLYNK_AUTH_TOKEN);
  timer.setInterval(2000L, kirimDataKeBlynkDanUpdateLCD);

  if (WiFi.status() == WL_CONNECTED) {
    wasConnected = true;
    lastSSID = WiFi.SSID();
  }

  bacaSemuaSensor();
  lcd.clear();
  tampilkanLayar();
}

void loop() {
  wifiManager.process();

  // --- LOGIKA PENDETEKSI & RECONNECT WIFI ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      lastSSID = WiFi.SSID(); 
      Blynk.connect(); 
      wifiManager.stopConfigPortal(); 
      tampilkanLayar(); 
    }
    Blynk.run();
  } else {
    if (wasConnected) {
      wasConnected = false;
      Blynk.disconnect(); 
      Serial.println("Koneksi terputus dari: " + lastSSID);
      
      // Paksa ESP32 memancarkan AP sekaligus mencari WiFi
      WiFi.mode(WIFI_AP_STA);
      wifiManager.startConfigPortal("SmartPlant_Setup");
      tampilkanLayar(); 
    }
    
    // --- FITUR AUTO-RECONNECT HALUS ---
    if (millis() - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = millis();
      if (lastSSID != "") {
        Serial.println("Mencoba menyambung kembali secara halus ke " + lastSSID + "...");
        WiFi.begin(); 
      }
    }
  }
  
  timer.run();

  // =========================================================
  // LOGIKA PUSHBUTTON (Pastikan hanya ada SATU blok ini saja)
  // =========================================================
  int reading = digitalRead(PIN_BUTTON);
  if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime > 50)) {
    lastDebounceTime = millis();
    currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
    
    // -- TRIK ANTI BERANTAKAN LCD --
    lcd.init();      // Inisialisasi ulang LCD
    lcd.backlight(); // Nyalakan ulang lampu belakang
    lcd.clear();     // Bersihkan layar
    
    tampilkanLayar();
  }
  lastButtonState = reading;
}

// Fungsi ini otomatis berjalan sesaat ketika tombol V5 di Blynk ditekan/berubah
// ============================================================================
// 5. LOGIKA TOMBOL BLYNK V6 (SIMULASI PUSHBUTTON FISIK)
// ============================================================================
BLYNK_WRITE(V6) {
  int statusTombol = param.asInt(); 

  // Hanya bereaksi ketika tombol ditekan (nilai 1)
  if (statusTombol == 1) {
    Serial.println("Tombol Blynk V6 ditekan: Ganti Layar LCD");
    
    // Logika ganti layar yang sama persis dengan tombol fisik
    currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
    lcd.clear();
    tampilkanLayar();
  }
}

// ============================================================================
// 6. FUNGSI MIRROR LCD KE BLYNK
// ============================================================================
void kirimTeksLcdKeBlynk(String baris1, String baris2) {
  if (Blynk.connected()) {
    Blynk.virtualWrite(V10, baris1); // Virtual Pin V10 -> Baris 1 LCD Widget
    Blynk.virtualWrite(V11, baris2); // Virtual Pin V11 -> Baris 2 LCD Widget
  }
}