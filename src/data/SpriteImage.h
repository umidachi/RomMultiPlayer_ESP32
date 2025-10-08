#pragma once
#include <vector>
#include <cstdint>

void displaySpriteImage(const std::vector<uint8_t>compressed) ;
void displaySpriteImageColor(const std::vector<uint8_t>compressed, const uint16_t* pal);
