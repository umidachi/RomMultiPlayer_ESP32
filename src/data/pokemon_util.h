#pragma once
#include <TFT_eSPI.h>
#include <arduino.h>
#include <vector>
#include <string>



uint8_t getPokemonSpriteBank(
    uint8_t indexNumber
);

std::vector<int> buildDexToIndex(
    const std::vector<uint8_t>& index_to_dex,
     size_t maxDex =256
);

std::vector<uint8_t> getPokemonName(
    const std::string &romPath, 
    uint8_t dex_id, 
    const std::vector<int> &dex_to_index
);

void getPokemonDexDetailFull(
    const std::string &romPath,
    uint8_t dex_id,
    const std::vector<int> &dex_to_index,
    std::vector<uint8_t> &dex_detail,
    std::vector<uint8_t> &poke_information_type,
    std::vector<uint8_t> &height_weight
);

std::vector<uint8_t> getCompressedPokemonSprite(
    const std::string &romPath,
    uint8_t dex_id,
    const std::vector<int> &dex_to_index
);

std::vector<uint16_t> getPokemonColorPalette(
    const std::string &romPath,
     uint8_t dex_id
); 
