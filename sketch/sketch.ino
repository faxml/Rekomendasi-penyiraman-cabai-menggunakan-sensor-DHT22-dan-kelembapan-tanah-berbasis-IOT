#include <ESP32Servo.h>

#define SERVO_FLAP 14
Servo flap;

void setup() {
  Serial.begin(115200);
  flap.attach(SERVO_FLAP, 500, 2400);
  flap.write(90);
  
  Serial.println("========================================");
  Serial.println("KALIBRASI SUDUT FLAP (MANUAL)");
  Serial.println("========================================");
  Serial.println("Ketik perintah di Serial Monitor:");
  Serial.println("  k = Gerakkan ke jalur KURANG");
  Serial.println("  o = Gerakkan ke jalur OPTIMAL");
  Serial.println("  Angka (0-180) = Cari sudut custom");
  Serial.println("Contoh: Ketik '45' lalu Enter");
  Serial.println("========================================");
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // Hapus spasi/enter kosong
    
    if (input == "k") {
      flap.write(137);
      Serial.println(">> Bergerak ke jalur KURANG (0°)");
    } 
    else if (input == "o") {
      flap.write(90);
      Serial.println(">> Bergerak ke jalur OPTIMAL (90°)");
    } 
    else {
      int sudut = input.toInt();
      if (sudut >= 0 && sudut <= 180) {
        flap.write(sudut);
        Serial.print(">> Bergerak ke sudut custom: ");
        Serial.print(sudut);
        Serial.println("°");
      } else {
        Serial.println(">> Perintah tidak dikenali. Gunakan 'k', 'o', atau angka 0-180.");
      }
    }
  }
}