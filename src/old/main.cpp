#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "PicUncompress.h"

Adafruit_MCP23X17 mcp;
TFT_eSPI tft = TFT_eSPI();

// ボタン接続
const int buttonPin = 0; // GPIO0 など
int lastButtonState = HIGH;
int currentImage = 0;

// 表示するファイル名のリスト
const char* files[] = {
  "/abra.pic",
  "/arbok.pic",
  "/pikachu.pic",
  "/ponyta.pic",
  "/tauros.pic"
};
const int numFiles = sizeof(files) / sizeof(files[0]);

// 幅・高さテーブル
struct SizeMap {
  int size;
  int width;
  int height;
};
SizeMap size_table[] = {
  {400, 40, 40},
  {576, 48, 48},
  {784, 56, 56}
};
void displayImage(int index) {
  File f = LittleFS.open(files[index], "r");
  if (!f) {
    Serial.printf("File %s not found\n", files[index]);
    return;
  }
  std::vector<uint8_t> compressed(f.size());
  f.read(compressed.data(), compressed.size());
  f.close();

  int out_size = uncompress(compressed);
  Serial.printf("Uncompressed size=%d bytes\n", out_size);

  int width=0, height=0;
  for (auto& m : size_table) {
    if (m.size == out_size) { width=m.width; height=m.height; break; }
  }
  if (width==0) { Serial.println("Unknown size"); return; }

  tft.fillScreen(TFT_WHITE);
  draw2bpp(output, width, height, 3);
}

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
  
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  // 最初の画像を表示
  displayImage(currentImage);
}

void loop() {
    static bool lastPressed[4] = {false,false,false,false};

    for(uint8_t i = 0; i < 4; i++){
        bool currentlyPressed = (mcp.digitalRead(i) == LOW); // LOWが押下
        // 前回は押されていなくて、今押された → エッジ検出
        if(currentlyPressed && !lastPressed[i]){
            currentImage = (currentImage + 1) % numFiles;
            displayImage(currentImage);
        }
        lastPressed[i] = currentlyPressed;
    }

    delay(20); // デバウンス対策
}


