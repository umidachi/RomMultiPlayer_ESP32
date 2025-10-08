#include "data/pokemon_util.h"
#include "data/rom_util.h"


extern TFT_eSPI tft;  // ← これを追加


// Dex番号 -> Index番号逆引き作成   
std::vector<int> buildDexToIndex(const std::vector<uint8_t>& index_to_dex, size_t maxDex){
    std::vector<int> dex_to_index(maxDex, -1);
    for (size_t i = 0; i < index_to_dex.size(); ++i) {
        uint8_t dex = index_to_dex[i];
        dex_to_index[dex] = i;
    }
    return dex_to_index;
}


uint8_t getPokemonSpriteBank(uint8_t indexNumber) {
    uint8_t spriteBank;

    if (indexNumber == 0x14) {
        spriteBank = 0x1;
    }
    else if (indexNumber == 0xB5) {
        spriteBank = 0xB;
    }
    else if (indexNumber < 0x1F) {
        spriteBank = 0x9;
    }
    else if (indexNumber < 0x49) {
        spriteBank = 0xA;
    }
    else if (indexNumber < 0x73) {
        spriteBank = 0xB;
    }
    else if (indexNumber < 0x98) {
        spriteBank = 0xC;
    }
    else {
        spriteBank = 0xD;
    }

    return spriteBank;
}

/**
 * @brief Dex番号からポケモンの名前（バイナリ配列）を取得する
 * 
 * @param romPath ROMファイルパス
 * @param dex_id Dex番号（0始まり）
 * @param dex_to_index Dex番号→Index番号逆引きテーブル
 * @return std::vector<uint8_t> ポケモン名のバイナリ配列
 */
std::vector<uint8_t> getPokemonName(const std::string &romPath, uint8_t dex_id, const std::vector<int> &dex_to_index) {
    const std::vector<uint8_t> stopByte = {0x50}; // 名前の終端コード
    const uint32_t nameTableAddress = 0x39446;    // ポケモン名前テーブルの先頭アドレス

    // Index番号をもとにROM内アドレスを計算
    uint32_t address = nameTableAddress + dex_to_index[dex_id] * 5;

    // 名前データを取得（最大5バイト）
    std::vector<uint8_t> pokename = readROMData(romPath, address, 5, stopByte);
    return pokename;
}

/**
 * @brief Dex番号を指定してポケモンの図鑑情報全体を取得する
 * 
 * @param romPath ROMファイルパス
 * @param dex_id Dex番号（0始まり）
 * @param dex_to_index Dex番号→Index番号逆引きテーブル
 * @param dex_detail 図鑑情報（説明文など）のバイナリ配列
 * @param poke_information_type タイプ情報のバイナリ配列
 * @param height_weight 高さ・重さ情報のバイナリ配列
 */
void getPokemonDexDetailFull(
    const std::string &romPath,
    uint8_t dex_id,
    const std::vector<int> &dex_to_index,
    std::vector<uint8_t> &dex_detail,
    std::vector<uint8_t> &poke_information_type,
    std::vector<uint8_t> &height_weight

) {
    const uint32_t dexPointerAddress = 0x4045B;
    const uint16_t dexPointerLength = 0x0405d7 - 0x4045B;
    std::vector<uint8_t> stopByte; // 今回は未使用

    // 1. Dexポインタデータを取得
    std::vector<uint8_t> dexpointer = readROMData(romPath, dexPointerAddress, dexPointerLength, stopByte);

    // 2. Dex番号からポケモン図鑑アドレスを取得
    uint32_t dex_address = readLittleEndian16(dexpointer, dex_to_index[dex_id] * 2);
    Serial.printf("index:%d\n",dex_to_index[dex_id]);
    // 3. ROM全体アドレスに変換
    dex_address = (dex_address - 0x4000) + 0x40000;
    Serial.printf("Dex Address: 0x%06X\n", dex_address);
 
    stopByte = {};
    dex_detail = readROMData(romPath, dex_address, 61, stopByte);
    Serial.printf("Dex detail size: %d\n", dex_detail.size());
    // 5. タイプ情報と高さ・重さを分割
    poke_information_type.clear();
    size_t i = 0;

    while (true) {
        if (dex_detail[i] == 0x50) {
            // 0x50（改行コード）までをbufに格納して削除
            dex_detail.erase(dex_detail.begin(), dex_detail.begin() + i);
            Serial.printf("Dex detail size after erase: %d\n", dex_detail.size());
            // 高さ・重さ情報を抜き出す（4バイト）
            height_weight.insert(height_weight.end(),dex_detail.begin()+1, dex_detail.begin() + 4);
            dex_detail.erase(dex_detail.begin(), dex_detail.begin() + 4);
            Serial.printf("Dex detail size after height/weight erase: %d\n", dex_detail.size());
            break;
        }
        // 0x50までをタイプ情報に格納
        poke_information_type.push_back(dex_detail[i]);
        Serial.printf("Type byte: 0x%02X\n", dex_detail[i]);
        i++;
    }
    i=0;

    while(i < dex_detail.size())  {  // 6. 図鑑情報の終端0x5Fまで削除    
            //0x5Fが入っていたら、そこまで削除して終了
           if (dex_detail[i] == 0x5F) {
            dex_detail.erase(dex_detail.begin() + i, dex_detail.end());
            break;
           }
           i++;
    }
           Serial.printf("Final Dex detail size: %d\n", dex_detail.size());
}

/**
 * @brief Dex番号からポケモンの圧縮スプライトデータを取得する
 * 
 * @param romPath ROMファイルパス
 * @param dex_id 取得したいポケモンのDex番号 (0始まり)
 * @param dex_to_index Dex番号→Index番号逆引きテーブル
 * @return std::vector<uint8_t> 圧縮されたスプライトデータ
 */
std::vector<uint8_t> getCompressedPokemonSprite(const std::string &romPath, uint8_t dex_id, const std::vector<int> &dex_to_index) {
    const uint32_t SpritePointerAddress = 0x383de;  // 一般ポケモンデータベースの先頭アドレス
    std::vector<uint8_t> stopByte;                  // 今回は未使用

    // 1. general_pokemonからポインタ情報を取得
    std::vector<uint8_t> general_pokemon = readROMData(
        romPath, 
        SpritePointerAddress + 28 * (dex_id - 1), 
        28, 
        stopByte
    );

    if (general_pokemon.size() < 13) {
        Serial.println("Error: general_pokemon データ不足");
        return {};
    }

    // 2. スプライトアドレスをリトルエンディアンで取得
    uint32_t sprite_address = readLittleEndian16(general_pokemon, 11); // 11要素目から2バイト
    Serial.printf("Sprite Address_pre convert: 0x%06X\n", sprite_address);

    // 3. スプライトBANKの決定
    uint8_t bank = getPokemonSpriteBank(dex_to_index[dex_id]);

    // 4. ROM全体アドレスに変換
    if(dex_id == 151){
    sprite_address = 0x4112;    // ミュウのスプライトアドレスは特例
    }else{
    sprite_address = (sprite_address - 0x4000) + bank * 0x4000;
    }

    Serial.printf("Sprite Address: 0x%06X\n", sprite_address);
    // 5. 圧縮されたスプライトデータを抽出
    std::vector<uint8_t> compressed_sprite = readROMData(romPath, sprite_address, 700, stopByte);

    return compressed_sprite;
}

std::vector<uint16_t> getPokemonColorPalette(const std::string &romPath, uint8_t dex_id) {
     // カラーパレット取得
    std::vector<uint8_t> stopByte;   
    std::vector<uint8_t> paletteIndex = readROMData(
        romPath, 0x72A0E , 150, stopByte
    );
    paletteIndex.push_back(paletteIndex[149]);  //Mewは151番目ですが、配列は150までしかないので、最後の要素をコピーして151番目にします。

    std::vector<uint8_t> PokemonPalette = readROMData(
        romPath, 0x72AA5 + 8 * paletteIndex[dex_id-1] , 8, stopByte
    );
    std::vector<uint16_t> binColors;

    for (size_t i = 0; i < 4; ++i) {
        uint16_t color = readLittleEndian16(PokemonPalette, i * 2);
        binColors.push_back(color);
    }

    std::vector<uint16_t> palette;
    palette.reserve(binColors.size());

   for (auto binColor : binColors) {
        // --- 1. 分解 ---
        uint8_t r = (binColor & 0b0000000000011111) >> 0;   // 下位5bit
        uint8_t g = (binColor & 0b0000001111100000) >> 5;   // 中間5bit
        uint8_t b = (binColor & 0b0111110000000000) >> 10;  // 上位5bit

        // --- 2. 0–255 に拡張 ---
        uint8_t r8 = r * 8;
        uint8_t g8 = g * 8;
        uint8_t b8 = b * 8;

        // --- 3. 再び RGB565 にパック ---
        // --- 3. TFT_eSPI の color565 で 16bit に変換 ---
        uint16_t packed = tft.color565(r8, g8, b8);
        //Serial.printf("R:%02X G:%02X B:%02X -> Packed: 0x%04X\n", r8, g8, b8, packed);
        palette.push_back(packed);
   }
   return palette;
}
