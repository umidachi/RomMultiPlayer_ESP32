#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>

// TFT 初期化
TFT_eSPI tft = TFT_eSPI();

// ------------------------- グローバル -------------------------
std::vector<uint8_t> output;
int cur_bit;
int cur_byte;

// ------------------------- ビット読み出し -------------------------
uint8_t read_bit(const std::vector<uint8_t>& data) {
  if (cur_bit == -1) {
    cur_byte++;
    cur_bit = 7;
  }
  return (data[cur_byte] >> cur_bit--) & 1;
}

int read_int(const std::vector<uint8_t>& data, int count) {
  int n = 0;
  while (count--) {
    n = (n << 1) | read_bit(data);
  }
  return n;
}

// ------------------------- タイル転置 -------------------------
void transpose_tiles(std::vector<uint8_t>& data, int width) {
  int size = width * width;
  for (int i = 0; i < size; i++) {
    int j = (i * width + i / width) % size;
    if (i < j) {
      uint8_t tmp[16];
      uint8_t* p = &data[i * 16];
      uint8_t* q = &data[j * 16];
      memcpy(tmp, p, 16);
      memcpy(p, q, 16);
      memcpy(q, tmp, 16);
    }
  }
}

// ------------------------- plane展開 -------------------------
std::vector<uint8_t> fill_plane(const std::vector<uint8_t>& data, int width) {
  static int table[16] = {
    0x0001,0x0003,0x0007,0x000F,0x001F,0x003F,0x007F,0x00FF,
    0x01FF,0x03FF,0x07FF,0x0FFF,0x1FFF,0x3FFF,0x7FFF,0xFFFF
  };
  int mode = read_bit(data);
  int size = width * width * 0x20;
  std::vector<uint8_t> plane(size);
  int len = 0;

  while (len < size) {
    if (mode) {
      while (len < size) {
        int bit_group = read_int(data, 2);
        if (!bit_group) break;
        plane[len++] = bit_group;
      }
    } else {
      size_t w = 0;
      while (read_bit(data)) w++;
      if (w >= 16) return {}; // エラー
      int n = table[w] + read_int(data, w + 1);
      while (len < size && n--) plane[len++] = 0;
    }
    mode ^= 1;
  }

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

// ------------------------- Grayコード復号 -------------------------
void uncompress_plane(std::vector<uint8_t>& plane, int width) {
  static int codes[2][16] = {
    {0x0,0x1,0x3,0x2,0x7,0x6,0x4,0x5,0xF,0xE,0xC,0xD,0x8,0x9,0xB,0xA},
    {0xF,0xE,0xC,0xD,0x8,0x9,0xB,0xA,0x0,0x1,0x3,0x2,0x7,0x6,0x4,0x5}
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

// ------------------------- uncompress -------------------------
int uncompress(const std::vector<uint8_t>& data) {
  cur_bit = 7;
  cur_byte = 0;
  int width = read_int(data, 4);
  if (read_int(data, 4) != width) return -1;

  int size = width * width * 8;
  std::vector<uint8_t> rams[2];
  output.resize(size * 2);

  int order = read_bit(data);
  rams[order] = fill_plane(data, width);

  int mode = read_bit(data);
  if (mode) mode += read_bit(data);

  rams[order ^ 1] = fill_plane(data, width);

  uncompress_plane(rams[order], width);
  if (mode != 1) uncompress_plane(rams[order ^ 1], width);

  if (mode != 0) {
    for (int i = 0; i < size; i++) {
      rams[order ^ 1][i] ^= rams[order][i];
    }
  }

  for (int i = 0; i < size; i++) {
    output[i * 2]     = rams[0][i];
    output[i * 2 + 1] = rams[1][i];
  }

  transpose_tiles(output, width);
  return size * 2;
}

// ------------------------- 2bpp描画 -------------------------
void draw2bpp(const std::vector<uint8_t>& data, int width, int height, int scale=1) {
  const uint16_t pal[4] = {0xFFFF, 0xAAAA, 0x5555, 0x0000};;
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
          tft.fillRect(px * scale, py * scale, scale, scale, pal[idx]);
        }
      }
    }
  }
}

// ------------------------- setup -------------------------
void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  // LittleFSから圧縮データ読み込み
  File f = LittleFS.open("/ponyta.pic", "r");
  if (!f) {
    Serial.println("File not found");
    return;
  }

  std::vector<uint8_t> compressed(f.size());
  f.read(compressed.data(), compressed.size());
  f.close();

  // 展開


  int size = uncompress(compressed);
  Serial.printf("Decompressed size=%d bytes\n", size);
  int pixel_count = size / 16;          // 2bppなのでピクセル数 
  int width_tiles = sqrt(pixel_count);
  int height_tiles = width_tiles; 

  // 描画 (例: 56x56px、倍率3)
  draw2bpp(output, width_tiles*8, height_tiles*8, 3);
}

void loop() {}
