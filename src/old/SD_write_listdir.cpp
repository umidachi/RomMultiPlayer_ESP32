#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();  
SPIClass spiSD(HSPI);

const int CS_PIN = 16;   // SDカードのCSピン
int16_t t_x, t_y;       // タッチ座標

uint16_t fileCount = 0; // 作成したファイルの連番カウンタ

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
  uint16_t x, y;

  tft.getTouchRaw(&x, &y);
  uint16_t z = tft.getTouchRawZ();

  if (z > 100) {   // タッチ判定の閾値
    Serial.printf("x: %i, y: %i, z: %i\n", x, y, z);

    // ファイル名を連番で作成
    fileCount++;
    char filename[20];
    sprintf(filename, "/file%u.txt", fileCount);

    File file = SD.open(filename, FILE_WRITE);
    if (file) {
      file.println("タッチで書き込みました");
      file.close();
      Serial.printf("SDに書き込み: %s\n", filename);
      tft.setCursor(0, tft.height() - 20);
      tft.printf("書き込み: %s  ", filename);
    } else {
      Serial.println("ファイル作成失敗");
      tft.setCursor(0, tft.height() - 20);
      tft.println("ファイル作成失敗  ");
    }

    delay(500); // 連打防止
  }

  delay(50); // ポーリング間隔
}
