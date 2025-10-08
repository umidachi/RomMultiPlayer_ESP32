#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

void setup() {
  Serial.begin(115200);
  Wire.begin(17,5); // SDA=21, SCL=22 (必要に応じて変更)

  // MCP23017 を I2C アドレス 0x20 で初期化
  mcp.begin_I2C(); // 0 は A2/A1/A0 = 0b000 -> アドレス 0x20

  // GPIOA0～GPIOA3 を入力に設定
  for (uint8_t i = 0; i < 4; i++) {
    mcp.pinMode(i, INPUT);
    //mcp.pullUp(i, HIGH); // 内部プルアップ有効
  }

  Serial.println("MCP23017 ボタン入力テスト開始");
}

void loop() {
  for (uint8_t i = 0; i < 4; i++) {
    int val = mcp.digitalRead(i);
    if (val == LOW) { // ボタン押下時
      Serial.printf("ボタン %d が押されました\n", i);
    }
  }
  delay(10); // デバウンスの簡易対策
}
