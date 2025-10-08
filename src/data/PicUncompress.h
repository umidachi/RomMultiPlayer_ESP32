#pragma once
#include <vector>
#include <stdint.h>


// グローバル出力バッファ
extern std::vector<uint8_t> output;


// 圧縮データを展開する関数
int uncompress(const std::vector<uint8_t>& data);

// 出力バッファを TFT に描画する関数
void draw2bpp(const std::vector<uint8_t>& data, int width, int height, int scale=1);

// 出力バッファを TFT に描画する関数
//void draw2bpp_color(const std::vector<uint8_t>& data, int width, int height, int scale=1,uint16_t const* palette=nullptr);
void draw2bpp_color(const std::vector<uint8_t>& data,
                    int width, int height, int scale,
                    const uint16_t* palette,
                    int x0 = 0, int y0 = 0); // ← デフォルト引数はここ