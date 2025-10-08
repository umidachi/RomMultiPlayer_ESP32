#include <Wire.h>
#include <Arduino.h>

void setup() {
  Wire.begin(17,5);           // SDA, SCL はデフォルトピン（GPIO21, GPIO22）
  Serial.begin(115200);
  Serial.println("\nI2Cスキャナを開始します...");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("スキャン中...");

  for (address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2Cデバイスを発見! アドレス: 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("不明なエラー at 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("デバイスは見つかりませんでした");
  else
    Serial.println("スキャン完了");

  delay(5000);  // 5秒ごとにスキャン
}
