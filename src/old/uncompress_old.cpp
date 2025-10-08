#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>

TFT_eSPI tft = TFT_eSPI();

// ======================================================
// ビットリーダー
// ======================================================
struct BitReader {
  const uint8_t* data;
  int curByte = 0;
  int curBit = 7;
  BitReader(const uint8_t* src) : data(src) {}
  int readBit() {
    int bit = (data[curByte] >> curBit) & 1;
    if (--curBit < 0) {
      curBit = 7;
      curByte++;
    }
    return bit;
  }
  int readInt(int n) {
    int val = 0;
    while (n--) {
      val = (val << 1) | readBit();
    }
    return val;
  }
};

// ======================================================
// タイルの転置処理 (pkmncompress.cのtranspose_tiles)
// ======================================================
void transpose_tiles(uint8_t* data, int width) {
  int size = width * width;
  for (int i = 0; i < size; i++) {
    int j = (i * width + i / width) % size;
    if (i < j) {
      uint8_t tmp[16];
      memcpy(tmp, data + i * 16, 16);
      memcpy(data + i * 16, data + j * 16, 16);
      memcpy(data + j * 16, tmp, 16);
    }
  }
}

// ======================================================
// fill_plane (圧縮→plane復号)
// ======================================================
std::vector<uint8_t> fill_plane(BitReader& br, int width) {
  static int table[16] = {
    0x0001, 0x0003, 0x0007, 0x000F,
    0x001F, 0x003F, 0x007F, 0x00FF,
    0x01FF, 0x03FF, 0x07FF, 0x0FFF,
    0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
  };

  int mode = br.readBit();
  int size = width * width * 0x20;
  std::vector<uint8_t> plane;
  plane.reserve(size);
  int len = 0;

  while (len < size) {
    if (mode) {
      while (len < size) {
        int bit_group = br.readInt(2);
        if (!bit_group) break;
        plane.push_back(bit_group);
        len++;
      }
    } else {
      size_t w = 0;
      while (br.readBit()) w++;
      if (w >= 16) {
        Serial.println("Invalid compressed data");
        return {};
      }
      int n = table[w] + br.readInt(w + 1);
      while (len < size && n--) {
        plane.push_back(0);
        len++;
      }
    }
    mode ^= 1;
  }

  // plane → ram変換
  std::vector<uint8_t> ram(size);
  len = 0;
  for (int y = 0; y < width; y++) {
    for (int x = 0; x < width * 8; x++) {
      for (int i = 0; i < 4; i++) {
        ram[len++] = plane[(y * 4 + i) * width * 8 + x];
      }
    }
  }

  for (int i = 0; i < size - 3; i += 4) {
    ram[i / 4] = (ram[i] << 6) | (ram[i + 1] << 4) | (ram[i + 2] << 2) | ram[i + 3];
  }
  ram.resize(size / 4);
  return ram;
}

// ======================================================
// uncompress_plane (Grayコード復号)
// ======================================================
void uncompress_plane(std::vector<uint8_t>& plane, int width) {
  static int codes[2][16] = {
    {0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5,
     0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA},
    {0xF, 0xE, 0xC, 0xD, 0x8, 0x9, 0xB, 0xA,
     0x0, 0x1, 0x3, 0x2, 0x7, 0x6, 0x4, 0x5},
  };

  for (int x = 0; x < width * 8; x++) {
    int bit = 0;
    for (int y = 0; y < width; y++) {
      int i = y * width * 8 + x;
      int nybble_hi = (plane[i] >> 4) & 0xF;
      int code_hi = codes[bit][nybble_hi];
      bit = code_hi & 1;
      int nybble_lo = plane[i] & 0xF;
      int code_lo = codes[bit][nybble_lo];
      bit = code_lo & 1;
      plane[i] = (code_hi << 4) | code_lo;
    }
  }
}

// ======================================================
// uncompress (メイン復号関数)
// ======================================================
int uncompress(const uint8_t* input, std::vector<uint8_t>& out) {
  BitReader br(input);

  int width = br.readInt(4);
  if (br.readInt(4) != width) {
    Serial.println("Not a square image");
    return -1;
  }

  int size = width * width * 8;

  int order = br.readBit();
  auto planeA = fill_plane(br, width);
  int mode = br.readBit();
  if (mode) mode += br.readBit();
  auto planeB = fill_plane(br, width);

  // Grayコード復号
  uncompress_plane(order ? planeB : planeA, width);
  if (mode != 1) uncompress_plane(order ? planeA : planeB, width);

  // XORモード
  if (mode != 0) {
    for (int i = 0; i < size; i++) {
      (order ? planeA[i] : planeB[i]) ^= (order ? planeB[i] : planeA[i]);
    }
  }

  // 2bpp 配列に変換
  out.resize(size * 2);
  for (int i = 0; i < size; i++) {
    out[i * 2]     = planeA[i];
    out[i * 2 + 1] = planeB[i];
  }

  transpose_tiles(out.data(), width);
  return out.size();
}

// ======================================================
// 展開した2bppデータを表示する関数 (倍率対応版)
// ======================================================
void draw2bpp(const std::vector<uint8_t>& data, int width, int height, int scale = 1) {
  const uint16_t pal[4] = {
    0xFFFF, // 白
    0x0000, // 黒
    0x8410, // 暗い灰
    0xC618 // 明るい灰

  };

  int tilesX = width / 8;
  int tilesY = height / 8;
  int tileIndex = 0;

  for (int ty = 0; ty < tilesY; ty++) {
    for (int tx = 0; tx < tilesX; tx++) {
      const uint8_t* tile = &data[tileIndex * 16];
      tileIndex++;
      for (int row = 0; row < 8; row++) {
        uint8_t lo = tile[row * 2];
        uint8_t hi = tile[row * 2 + 1];
        for (int col = 0; col < 8; col++) {
          int bit = 7 - col;
          uint8_t idx = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1);
          int px = tx * 8 + col;
          int py = ty * 8 + row;

          // 倍率に応じて塗りつぶし矩形を描画
          tft.fillRect(px * scale, py * scale, scale, scale, pal[idx]);
        }
      }
    }
  }
}

// ======================================================
// Setup
// ======================================================
void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  // 圧縮データを読み込む
  //File f = LittleFS.open("/abra.pic", "r");
 // File f = LittleFS.open("/pikachu.pic", "r");
//  File f = LittleFS.open("/ponytaq.pic", "r");
  File f = LittleFS.open("/tauros.pic", "r");
  if (!f) {
    Serial.println("File not found");
    return;
  }
  std::vector<uint8_t> compressed(f.size());
  f.read(compressed.data(), compressed.size());
  f.close();

  // 展開
  std::vector<uint8_t> decompressed;
  int size = uncompress(compressed.data(), decompressed);
  Serial.printf("Decompressed size=%d bytes\n", size);

  // 例: 幅40px, 高さ40pxのスプライトを描画
  draw2bpp(decompressed, 56, 56,3);
}

void loop() {}
