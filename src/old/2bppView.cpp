// 2bpp_tile_view.ino
// ESP32 + LittleFS + TFT_eSPI 用フルスケッチ
// タイル形式 (8x8, 2bpp, 16 bytes/tile) のバイナリを LittleFS から読み込み ILI9341 に表示する

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector> 

TFT_eSPI tft = TFT_eSPI();

// パレット（RGB565）
static const uint16_t palette[4] = {
   0xFFFF, // 255 → 白
    0xC618, // 170 → 明るい灰
    0x8410, // 85  → 暗い灰
    0x0000  // 0   → 黒
};

// ファイルを丸ごと読み込むユーティリティ（戻り値は false なら読み込み失敗）
bool readFileToBuffer(const char *path, std::vector<uint8_t> &out) {
  fs::File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("ファイルオープン失敗: %s\n", path);
    return false;
  }
  size_t sz = f.size();
  if (sz == 0) {
    Serial.printf("ファイルサイズ0: %s\n", path);
    f.close();
    return false;
  }

  out.resize(sz);
  size_t readBytes = f.read(out.data(), sz);
  f.close();

  if (readBytes != sz) {
    Serial.printf("読み取りバイト数不一致: %u != %u\n", (unsigned)readBytes, (unsigned)sz);
    return false;
  }
  return true;
}

// タイル形式データを描画する関数
// path: LittleFS 上のファイルパス
// dstX, dstY: 描画先左上座標
// width, height: 出力画像サイズ (必ず 8 の倍数)
// pal: 4要素のRGB565パレット
void drawTilesFromFile(const char *path, int16_t dstX, int16_t dstY, int width, int height, const uint16_t pal[4]) {
  if ((width % 8) != 0 || (height % 8) != 0) {
    Serial.println("width/height must be multiple of 8");
    return;
  }

  std::vector<uint8_t> data;
  if (!readFileToBuffer(path, data)) return;

  const int tilesX = width / 8;
  const int tilesY = height / 8;
  const size_t expectedSize = (size_t)tilesX * tilesY * 16; // 16 bytes per tile
  if (data.size() < expectedSize) {
    Serial.printf("データサイズ不足: %u < %u\n", (unsigned)data.size(), (unsigned)expectedSize);
    return;
  }

  // ILI9341へ描画するためにアドレスウィンドウを設定。
  tft.startWrite();
  tft.setAddrWindow(dstX, dstY, width, height);

  // 1行分の色バッファ（最大幅 240）
  // 動的確保だが、ILI9341の最大横幅は240なのでそれを超えないこと
  if (width > 240) {
    Serial.println("幅が大きすぎます（最大240）");
    tft.endWrite();
    return;
  }
  uint16_t lineBuffer[240];

  // データはタイル単位で (ty, tx) の順、各タイルは row=0..7 の各行で (byte1, byte2)
  // タイルオフセットの計算に使う
  auto tileOffset = [&](int tx, int ty) -> size_t {
    return (size_t)((ty * tilesX + tx) * 16); // 16 bytes per tile
  };

  // 画面上の各タイル行ごとに処理する（上から順）
  for (int ty = 0; ty < tilesY; ty++) {
    // タイル内の行 0..7 を順に描く（これで画像の次の8行を生成）
    for (int rowInTile = 0; rowInTile < 8; rowInTile++) {
      int bufIndex = 0;
      // 各タイル列を横に並べてその行のバイトを取得して並べる
      for (int tx = 0; tx < tilesX; tx++) {
        size_t toff = tileOffset(tx, ty) + (size_t)rowInTile * 2; // 2 bytes per row
        uint8_t byte1 = data[toff];       // plane 0 (low)
        uint8_t byte2 = data[toff + 1];   // plane 1 (high)

        // 8ピクセル分を左から順に取り出す
        for (int col = 0; col < 8; col++) {
          int bit = 7 - col;
          uint8_t lo = (byte1 >> bit) & 1;
          uint8_t hi = (byte2 >> bit) & 1;
          uint8_t idx = (hi << 1) | lo; // 0..3
          lineBuffer[bufIndex++] = pal[idx];
        }
      }

      // その行バッファを一気に送る
      // pushColors( buffer, length, swapBytes ) ; TFT_eSPI では最後の引数を true にすることが多い
      tft.pushColors(lineBuffer, bufIndex, true);
    }
  }

  tft.endWrite();
  Serial.printf("描画完了: %s (%dx%d)\n", path, width, height);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS begin failed!");
    while (1) delay(1000);
  }
  Serial.println("LittleFS mounted.");

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // 使い方の例:
  // LittleFS に "/tiles.bin" を入れておくこと。
  // 例えば幅=128, 高さ=128 の画像であれば tilesX=16, tilesY=16 -> サイズ = 16*16*16 = 4096 bytes
  // 必要に応じて palette を書き換えてください。
  drawTilesFromFile("/abra.2bpp", 0, 0, 40, 40, palette);
}

void loop() {
  // 特に何もしない
  delay(1000);
}
