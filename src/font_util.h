#include <Arduino.h>
#include <TFT_eSPI.h>
#include <map>
#include <stdint.h>
#include <Vector>

extern uint16_t textColor;
extern uint16_t bgColor;
static const std::map<uint8_t, FontInfo> fontTable;

void drawFont8x8(TFT_eSPI &tft, int x, int y, uint8_t buf[8], uint16_t color, uint16_t bg, uint8_t scale);
void drawKanaStacked(TFT_eSPI &tft, const std::string &romPath, uint8_t code, int x, int y, uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t scale = 2);
void drawBinaryString(TFT_eSPI &tft, const std::vector<uint8_t>& data, int startX, int startY, int spacing, uint8_t scale, const std::string &romPath);
