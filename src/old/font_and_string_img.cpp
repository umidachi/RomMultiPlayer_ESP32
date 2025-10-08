#include <Arduino.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>
#include <string>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "PicUncompress.h"
#include "Font_table.h"  // fontTable が定義されている

TFT_eSPI tft = TFT_eSPI();

Adafruit_MCP23X17 mcp;
//TFT_eSPI tft = TFT_eSPI();

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

  //tft.fillScreen(TFT_WHITE);
  draw2bpp(output, width, height, 3);
}


// --- ROM からバイナリ取得 ---
std::vector<uint8_t> readROMData(const std::string &path, uint32_t startAddr, size_t maxLength, const std::vector<uint8_t>& stopSequence = {}) {
    std::vector<uint8_t> result;

    File rom = LittleFS.open(path.c_str(), "r");
    if (!rom) {
        Serial.println("ROM ファイル開けません");
        return result;
    }

    rom.seek(startAddr, SeekSet);

    size_t readCount = 0;
    std::vector<uint8_t> buffer;

    while (readCount < maxLength && rom.available()) {
        uint8_t byte = rom.read();
        result.push_back(byte);
        readCount++;

        Serial.print("ROM byte: 0x");
        Serial.println(byte, HEX); // デバッグ出力

        if (!stopSequence.empty()) {
            buffer.push_back(byte);
            if (buffer.size() > stopSequence.size()) buffer.erase(buffer.begin());

            if (buffer.size() == stopSequence.size()) {
                bool match = true;
                for (size_t i = 0; i < stopSequence.size(); i++) {
                    if (buffer[i] != stopSequence[i]) { match = false; break; }
                }
                if (match) break;
            }
        }
    }

    rom.close();
    return result;
}

// --- 8x8 フォント描画 ---
void drawFont8x8(TFT_eSPI &tft, int x, int y, uint8_t buf[8], uint16_t color, uint16_t bg, uint8_t scale) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            bool bit = (buf[row] >> (7 - col)) & 1;
            uint16_t c = bit ? color : bg;
            if (scale == 1) {
                tft.drawPixel(x + col, y + row, c);
            } else {
                tft.fillRect(x + col*scale, y + row*scale, scale, scale, c);
            }
        }
    }
}

// --- 上文字＋ベース文字描画（デバッグ入り） ---
void drawKanaStacked(TFT_eSPI &tft, const std::string &romPath, uint8_t code, int x, int y, uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t scale = 2) {
    auto it = fontTable.find(code);
    if (it == fontTable.end()) {
        Serial.print("FontTable に存在しないコード: 0x");
        Serial.println(code, HEX);
        return;
    }

    FontInfo info = it->second;
    Serial.print("コード 0x"); Serial.print(code, HEX);
    Serial.print(" → base: 0x"); Serial.print(info.baseAddress, HEX);
    Serial.print(", accent: 0x"); Serial.println(info.accentAddress, HEX);

    uint8_t buf[8];
    File rom = LittleFS.open(romPath.c_str(), "r");
    if (!rom) {
        Serial.println("ROM ファイル開けません(drawKanaStacked)");
        return;
    }

    // 上文字
    if (info.accentAddress != 0) {
        rom.seek(info.accentAddress, SeekSet);
        if (rom.read(buf, 8) == 8) {
            Serial.println("上文字描画");
            drawFont8x8(tft, x, y, buf, color, bg, scale);
        } else {
            Serial.println("上文字読み込み失敗");
        }
    }

    // ベース文字
    rom.seek(info.baseAddress, SeekSet);
    if (rom.read(buf, 8) == 8) {
        Serial.println("ベース文字描画");
        drawFont8x8(tft, x, y + 8*scale, buf, color, bg, scale);
    } else {
        Serial.println("ベース文字読み込み失敗");
    }

    rom.close();
}

// --- バイナリ配列描画 ---
void drawBinaryString(TFT_eSPI &tft, const std::vector<uint8_t>& data, int startX, int startY, int spacing, uint8_t scale, const std::string &romPath) {
    int x = startX;
    int y = startY;

    for (auto code : data) {
        if (code == 0x4E || code == 0x4F) { x = startX; y += 16*scale + spacing; continue; }
        if (code == 0x7F) { x += 8*scale + spacing; continue;}

        drawKanaStacked(tft, romPath, code, x, y, TFT_WHITE, TFT_BLACK, scale);
        x += 8*scale + spacing;

        if (x + 8*scale > tft.width()) { x = startX; y += 16*scale + spacing; }
    }
}

// --- main ---
void setup() {
    Serial.begin(115200);
    delay(500); // シリアル安定化
    Wire.begin(17,5); // SDA=17, SCL=5 (必要に応じて変更)
    // MCP23017 を I2C アドレス 0x20 で初期化
    mcp.begin_I2C(); // 0 は A2/A1/A0 = 0b000 -> アドレス 0x20
    // GPIOA0～GPIOA3 を入力に設定
   for (uint8_t i = 0; i < 4; i++) {
    mcp.pinMode(i, INPUT);
    //mcp.pullUp(i, HIGH); // 内部プルアップ有効
  }


    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS 初期化失敗");
        return;
    }

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    const std::string romPath = "/pokemon_blue.gb";

    std::vector<uint8_t> stopByte = {0x5f};
    std::vector<uint8_t> data = readROMData(romPath, 0x4060E +7,100, stopByte);

    Serial.print("取得バイト数: "); Serial.println(data.size());

    drawBinaryString(tft, data, 0, 150, 2, 1, romPath);
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
