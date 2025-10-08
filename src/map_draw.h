#pragma once
//#include <map>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <string>
#include <cstdint>
#include <vector>
//#include "map_table.h" // mapData が定義されている
#define MAP_W 40
#define MAP_H 30

extern std::vector<const uint8_t*> tileset;
extern TFT_eSPI tft; 
extern uint16_t gb_palette[4];
extern uint8_t mapData[MAP_H][MAP_W];

void decodeTile2bpp(const uint8_t* tileData, uint16_t* outBuf);
void drawMap();
void drawTileAt(int x, int y, uint8_t tileNum);




