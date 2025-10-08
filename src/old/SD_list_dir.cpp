#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();  
SPIClass spiSD(HSPI);

const int CS_PIN = 16;   // SDカードのCSピン
int16_t t_x, t_y;       // タッチ座標

// ディレクトリ内容を再帰的に表示
void listDir(fs::FS &fs, const char * dirname, uint8_t levels, int16_t yStart = 40) {
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) {
    tft.setCursor(0, yStart);
    tft.println("フォルダを開けませんでした");
    return;
  }

  File file = root.openNextFile();
  int16_t y = yStart;
  while (file) {
    if (file.isDirectory()) {
      tft.setCursor(0, y);
      tft.printf("[DIR] %s\n", file.name());
      y += 16;
      if (levels) {
        listDir(fs, file.name(), levels - 1, y);
      }
    } else {
      tft.setCursor(0, y);
      tft.printf("%s (%d bytes)\n", file.name(), file.size());
      y += 16;
    }
    file = root.openNextFile();
  }
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  spiSD.begin(12, 14, 27, 16);  // SCK, MISO, MOSI, CS
  if (!SD.begin(CS_PIN, spiSD)) {
    tft.println("SDカード初期化失敗");
    return;
  }
  tft.println("SDカード初期化成功");
  delay(1000);
  tft.fillScreen(TFT_BLACK);

  listDir(SD, "/", 2); // 階層2まで表示
}

void loop() {
  // タッチされていたら座標をシリアルに出力
uint16_t x, y;

 tft.getTouchRaw(&x, &y);

 if (tft.getTouchRawZ() > 100) {   // 適当な閾値
    // タッチとみなす
      Serial.printf("x: %i     ", x);
      Serial.printf("y: %i     \n", y);
    }
 delay(250);


}
