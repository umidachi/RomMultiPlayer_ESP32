#include "data/SpriteImage.h"
#include "data/PicUncompress.h"
#include <Arduino.h>

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

void displaySpriteImage(const std::vector<uint8_t>compressed) {
  int out_size = uncompress(compressed);
  Serial.printf("Uncompressed size=%d bytes\n", out_size);

  int width=0, height=0;
  for (auto& m : size_table) {
    if (m.size == out_size) { width=m.width; height=m.height; break; }
  }
  if (width==0) { Serial.println("Unknown size"); return; }

  draw2bpp(output, width, height, 2);
}

void displaySpriteImageColor(const std::vector<uint8_t>compressed, const uint16_t* pal) {
  int out_size = uncompress(compressed);
  Serial.printf("Uncompressed size=%d bytes\n", out_size);

  int width=0, height=0;
  for (auto& m : size_table) {
    if (m.size == out_size) { width=m.width; height=m.height; break; }
  }
  if (width==0) { Serial.println("Unknown size"); return; }

  draw2bpp_color(output, width, height, 2, pal,10,10);
  

}