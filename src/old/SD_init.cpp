#include <SPI.h>
#include <SD.h>

SPIClass spiSD(VSPI);  // VSPIを使う独立バス
const int CS_PIN = 16;

void setup() {
  Serial.begin(115200);
  spiSD.begin(12, 14, 27, 16); // SCK, MISO, MOSI, CS

  if(!SD.begin(CS_PIN, spiSD)){
    Serial.println("SDカード初期化失敗");
    return;
  }
  Serial.println("SDカード初期化成功");
}

void loop() {}
