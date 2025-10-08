#include <Arduino.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>
#include <string>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "PicUncompress.h"
#include "Font_table.h"  // fontTable が定義されている

TFT_eSPI tft = TFT_eSPI();

Adafruit_MCP23X17 mcp;
//TFT_eSPI tft = TFT_eSPI();

// ボタン接続
const int buttonPin = 0; // GPIO0 など
int lastButtonState = HIGH;
int currentImage = 0;





//ポケモン図鑑情報　index,dex
std::vector<uint8_t> index_to_dex;
std::vector<int> dex_to_index;
//呼び出すロム情報
const std::string romPath = "/pokemon_blue.gb";
//ポケモン図鑑の表示するDex番号
static uint8_t dex_id = 1;

// フォントの色設定　白地（背景）に黒文字
uint16_t textColor = TFT_BLACK;   // 文字の色
uint16_t bgColor   = TFT_WHITE;   // 背景の色

// 表示するファイル名のリスト
const char* files[] = {
  "/abra.pic",
  "/arbok.pic",
  "/pikachu.pic",
  "/ponyta.pic",
  "/tauros.pic"
};
const int numFiles = sizeof(files) / sizeof(files[0]);

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



// --- ROM からバイナリ取得 ---
std::vector<uint8_t> readROMData(const std::string &path, uint32_t startAddr, size_t maxLength, const std::vector<uint8_t>& stopSequence = {}) {
    std::vector<uint8_t> result;

    File rom = LittleFS.open(path.c_str(), "r");
    if (!rom) {
        Serial.println("ROM ファイル開けません");
        return result;
    }

    rom.seek(startAddr, SeekSet);

    size_t readCount = 0;
    std::vector<uint8_t> buffer;

    while (readCount < maxLength && rom.available()) {
        uint8_t byte = rom.read();
        result.push_back(byte);
        readCount++;

        //Serial.print("ROM byte: 0x");
        //Serial.println(byte, HEX); // デバッグ出力

        if (!stopSequence.empty()) {
            buffer.push_back(byte);
            if (buffer.size() > stopSequence.size()) buffer.erase(buffer.begin());

            if (buffer.size() == stopSequence.size()) {
                bool match = true;
                for (size_t i = 0; i < stopSequence.size(); i++) {
                    if (buffer[i] != stopSequence[i]) { match = false; break; }
                }
                if (match) break;
            }
        }
    }

    rom.close();
    return result;
}
// vector対応リトルエンディアン16bit読み込み
uint16_t readLittleEndian16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 1 >= data.size()) throw std::out_of_range("readLittleEndian16: offset out of range");
    return data[offset] | (data[offset + 1] << 8);
}

// Dex番号 -> Index番号逆引き作成   
std::vector<int> buildDexToIndex(const std::vector<uint8_t>& index_to_dex, size_t maxDex = 256) {
    std::vector<int> dex_to_index(maxDex, -1);
    for (size_t i = 0; i < index_to_dex.size(); ++i) {
        uint8_t dex = index_to_dex[i];
        dex_to_index[dex] = i;
    }
    return dex_to_index;
}

// --- 8x8 フォント描画 ---
void drawFont8x8(TFT_eSPI &tft, int x, int y, uint8_t buf[8], uint16_t color, uint16_t bg, uint8_t scale) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            bool bit = (buf[row] >> (7 - col)) & 1;
            uint16_t c = bit ? color : bg;
            if (scale == 1) {
                tft.drawPixel(x + col, y + row, c);
            } else {
                tft.fillRect(x + col*scale, y + row*scale, scale, scale, c);
            }
        }
    }
}

// --- 上文字＋ベース文字描画（デバッグ入り） ---
void drawKanaStacked(TFT_eSPI &tft, const std::string &romPath, uint8_t code, int x, int y, uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t scale = 2) {
    auto it = fontTable.find(code);
    if (it == fontTable.end()) {
        Serial.print("FontTable に存在しないコード: 0x");
        Serial.println(code, HEX);
        return;
    }

    FontInfo info = it->second;
    Serial.print("コード 0x"); Serial.print(code, HEX);
    Serial.print(" → base: 0x"); Serial.print(info.baseAddress, HEX);
    Serial.print(", accent: 0x"); Serial.println(info.accentAddress, HEX);

    uint8_t buf[8];
    File rom = LittleFS.open(romPath.c_str(), "r");
    if (!rom) {
        Serial.println("ROM ファイル開けません(drawKanaStacked)");
        return;
    }

    // 上文字
    if (info.accentAddress != 0) {
        rom.seek(info.accentAddress, SeekSet);
        if (rom.read(buf, 8) == 8) {
            Serial.println("上文字描画");
            drawFont8x8(tft, x, y, buf, color, bg, scale);
        } else {
            Serial.println("上文字読み込み失敗");
        }
    }

    // ベース文字
    rom.seek(info.baseAddress, SeekSet);
    if (rom.read(buf, 8) == 8) {
        Serial.println("ベース文字描画");
        drawFont8x8(tft, x, y + 8*scale, buf, color, bg, scale);
    } else {
        Serial.println("ベース文字読み込み失敗");
    }

    rom.close();
}


// --- バイナリ配列描画 ---
void drawBinaryString(TFT_eSPI &tft, const std::vector<uint8_t>& data, int startX, int startY, int spacing, uint8_t scale, const std::string &romPath) {
    int x = startX;
    int y = startY;

    for (auto code : data) {
        if (code == 0x4E || code == 0x4F) { x = startX; y += 16*scale + spacing; continue; }
        if (code == 0x7F) { x += 8*scale + spacing; continue;}

        drawKanaStacked(tft, romPath, code, x, y, textColor, bgColor, scale);
        x += 8*scale + spacing;

        if (x + 8*scale > tft.width()) { x = startX; y += 16*scale + spacing; }
    }
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
 * @brief Dex番号からポケモン図鑑情報（タイプ、高さ・重さなど）を取得する
 * 
 * @param romPath ROMファイルパス
 * @param dex_id Dex番号（0始まり）
 * @param dex_to_index Dex番号→Index番号逆引きテーブル
 * @param dex_detail 出力：図鑑の詳細データ
 * @param poke_information_type 出力：タイプ情報
 */
void getPokemonDexDetail(
    const std::string &romPath,
    uint8_t dex_id,
    const std::vector<int> &dex_to_index,
    std::vector<uint8_t> &dex_detail,
    std::vector<uint8_t> &poke_information_type
) {
    const uint32_t dexPointerAddress = 0x4045B;
    const uint16_t dexPointerLength = 0x0405d7 - 0x4045B;
    std::vector<uint8_t> stopByte; // 今回は未使用

    // 1. Dexポインタデータを取得
    std::vector<uint8_t> dexpointer = readROMData(romPath, dexPointerAddress, dexPointerLength, stopByte);

    // 2. Dex番号からポケモン図鑑アドレスを取得
    uint32_t dex_address = readLittleEndian16(dexpointer, dex_to_index[dex_id] * 2);
    Serial.printf("index:%d",dex_to_index[dex_id]);
    // 3. ROM全体アドレスに変換
    dex_address = (dex_address - 0x4000) + 0x40000;

    // 4. 図鑑データを取得（stopByte = 0x5F）
    stopByte = {0x5F};
    dex_detail = readROMData(romPath, dex_address, 100, stopByte);

    // 5. タイプ情報と高さ・重さを分割
    poke_information_type.clear();
    size_t i = 0;

    while (true) {
        if (dex_detail[i] == 0x50) {
            // 0x50（改行コード）までをbufに格納して削除
            dex_detail.erase(dex_detail.begin(), dex_detail.begin() + i);

            // 高さ・重さ情報を抜き出す（4バイト）
            std::vector<uint8_t> height_weight(dex_detail.begin(), dex_detail.begin() + 4);
            dex_detail.erase(dex_detail.begin(), dex_detail.begin() + 4);

            break;
        }
        // 0x50までをタイプ情報に格納
        poke_information_type.push_back(dex_detail[i]);
        i++;
    }
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

/**
 * @brief Dex番号を指定してポケモンの名前・図鑑情報・圧縮スプライトを取得して描画する
 * 
 * @param romPath ROMファイルパス
 * @param tft TFTディスプレイオブジェクト
 * @param dex_id Dex番号（0始まり）
 */

void displayPokemonInfo(const std::string &romPath, TFT_eSPI &tft, uint8_t dex_id,
                        const std::vector<int> &dex_to_index) {


    bgColor = tft.color565(248, 232, 248); // fontの背景色をポケモンの色パレットに合わせる。
    // 名前取得
    std::vector<uint8_t> pokename = getPokemonName(romPath, dex_id, dex_to_index);

    // 描画
    drawBinaryString(tft, pokename, 170, 30, 2, 2, romPath);

    // スプライト取得
    std::vector<uint8_t> compressed_sprite = getCompressedPokemonSprite(romPath, dex_id, dex_to_index);
    
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

    // 例: 2byteの4要素（RGB565）
    //std::vector<uint16_t> binColors = {0x7FBF, 0x4354, 0x2389, 0x843};

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

    displaySpriteImageColor(compressed_sprite,palette.data());


    // 図鑑詳細取得
    std::vector<uint8_t> dex_detail;
    std::vector<uint8_t> poke_information_type;
    getPokemonDexDetail(romPath, dex_id, dex_to_index, dex_detail, poke_information_type);

    //種族　170, 30, 2, 2
    drawBinaryString(tft, poke_information_type, 170, 78, 2, 1, romPath);


    drawBinaryString(tft, dex_detail, 20, 140, 2, 1, romPath);

    
}



// --- main ---
void setup() {
    Serial.begin(115200);
    delay(500); // シリアル安定化
    Wire.begin(17,5); // SDA=17, SCL=5 (必要に応じて変更)
    // MCP23017 を I2C アドレス 0x20 で初期化
    mcp.begin_I2C(); // 0 は A2/A1/A0 = 0b000 -> アドレス 0x20
    // GPIOA0～GPIOA3 を入力に設定
   for (uint8_t i = 0; i < 4; i++) {
    mcp.pinMode(i, INPUT);
    //mcp.pullUp(i, HIGH); // 内部プルアップ有効
  }


    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS 初期化失敗");
        return;
    }

    tft.init();
    tft.setRotation(1);
    uint16_t myColor = tft.color565(248, 232, 248); // 白紫系
    tft.fillScreen(myColor);


    //tft.fillScreen(TFT_WHITE);

    //const std::string romPath = "/pokemon_blue.gb";
    const std::string romPath = "/pokemon_blue.gb";
    // 1度だけ読み込む
    std::vector<uint8_t> stopByte; // 今回は未使用(空)

    index_to_dex = readROMData(romPath, 0x42784, 190, stopByte);
    dex_to_index = buildDexToIndex(index_to_dex);
    displayPokemonInfo(romPath,tft,dex_id, dex_to_index);
}

void loop() {
    static uint8_t dex_id = 1;                   // Dex番号: 1～151
    static bool lastPressed[4] = {false,false,false,false};

    for (uint8_t i = 0; i < 4; i++) {
        bool currentlyPressed = (mcp.digitalRead(i) == LOW); // LOWが押下

        // 押した瞬間だけ反応（エッジ検出）
        if (currentlyPressed && !lastPressed[i]) {

            // 画面全体を白でクリア
            uint16_t myColor = tft.color565(248, 232, 248); // 白紫系
            tft.fillScreen(myColor);
            //tft.fillScreen(TFT_WHITE);

            // ボタンごとの処理
            switch(i) {
                case 0: // ボタン1: 1単位でインクリメント
                    dex_id++;
                    if (dex_id > 151) dex_id = 1;
                    break;

                case 1: // ボタン2: 1単位でデクリメント
                    if (dex_id == 1) dex_id = 151;
                    else dex_id--;
                    break;

                case 2: // ボタン3: 10単位でインクリメント
                    dex_id += 10;
                    if (dex_id > 151) dex_id -= 151;
                    break;

                case 3: // ボタン4: 10単位でデクリメント
                    if (dex_id <= 10) dex_id = 151 - (10 - dex_id);
                    else dex_id -= 10;
                    break;
            }

            // 選択したDex番号のポケモン情報を表示
            displayPokemonInfo(romPath, tft, dex_id, dex_to_index);
        }

        // 現在の押下状態を保存
        lastPressed[i] = currentlyPressed;
    }

    delay(20); // デバウンス対策
}
