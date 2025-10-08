#include <Arduino.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>
#include <string>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "rom_util.h"
#include "PicUncompress.h"
#include "pokemon_util.h"
#include "map_table.h" // mapData が定義されている
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
std::map<std::string, uint8_t> string2Byte;

//呼び出すロム情報
const std::string romPath = "/pokemon_blue.gb";

//ポケモン図鑑の表示するDex番号
static uint8_t dex_id = 1; 

// フォントの色設定　白地（背景）に黒文字
uint16_t textColor = TFT_BLACK;   // 文字の色
uint16_t bgColor   = TFT_WHITE;   // 背景の色


// 幅・高さテーブル
//ポケモンのスプライト複合時のファイルサイズによる出力（縦横）を決める
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


std::vector<const uint8_t*> tileset;

// Game Boy風 4色パレット (RGB565)
uint16_t gb_palette[4] = {
  0xFFFF, // 白
  0xC618, // 薄い灰色
  0x7BEF, // 濃い灰色
  0x0000
};

// ----------------------------
// タイルをRGB565バッファに展開
// ----------------------------
void decodeTile2bpp(const uint8_t* tileData, uint16_t* outBuf) {
  const int tile_size = 8;
  for (int row = 0; row < tile_size; row++) {
    uint8_t lo = tileData[row * 2];
    uint8_t hi = tileData[row * 2 + 1];
    for (int col = 0; col < tile_size; col++) {
      uint8_t bit0 = (lo >> (7 - col)) & 1;
      uint8_t bit1 = (hi >> (7 - col)) & 1;
      uint8_t colorIndex = (bit1 << 1) | bit0;
      outBuf[row * tile_size + col] = gb_palette[colorIndex];
    }
  }
}
// ----------------------------
// マップを一気に描画
// ----------------------------
void drawMap() {
    const int TILE = 8;
    uint16_t tileBuf[TILE * TILE];

    for (int my = 0; my < MAP_H; my++) {
        for (int mx = 0; mx < MAP_W; mx++) {
            uint8_t tileNum = mapData[my][mx];

            // 空白タイルなら描画せずスキップ
            if (tileNum == 0) continue;

            // タイルをデコードしてバッファに展開
            decodeTile2bpp(tileset[tileNum], tileBuf);

            // 一気に描画
            tft.pushImage(mx * TILE, my * TILE, TILE, TILE, tileBuf);
        }
    }
}

void drawTileAt(int x, int y, uint8_t tileNum) {
    const int TILE = 8;
    uint16_t tileBuf[TILE * TILE];

    // タイルデコード
    decodeTile2bpp(tileset[tileNum], tileBuf);

    // ILI9341 に描画
    tft.pushImage(x, y, TILE, TILE, tileBuf);
}



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


void buildTileSet() {
    // タイルデータの読み込み
    std::vector<uint8_t> tileset_buf =
        readROMData(romPath, 0x12881, 320, {});
        
    for (size_t i=0; i + 16 <= tileset_buf.size(); i += 16) {
        uint8_t* tile = new uint8_t[16];
        std::copy(tileset_buf.begin()+i, tileset_buf.begin()+i+16, tile);
        tileset.push_back(tile);
    }
}


// 文字 → バイナリ値 の逆引きテーブル
static std::map<std::string, uint8_t> BuildChar2ByteTable() {
    std::map<std::string, uint8_t> reverse;

    for (const auto& pair : fontTable) {
        uint8_t code = pair.first;           // キー
        const FontInfo& info = pair.second;  // 値
        reverse[info.character] = code;
    }
    return reverse;
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


// UTF-8文字列を1文字ずつ分割する関数
std::vector<std::string> splitUTF8(const std::string& str) {
    std::vector<std::string> result;
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        size_t charLen = 0;

        if ((c & 0x80) == 0x00)       charLen = 1; // ASCII
        else if ((c & 0xE0) == 0xC0)  charLen = 2; // 2バイト
        else if ((c & 0xF0) == 0xE0)  charLen = 3; // 3バイト
        else if ((c & 0xF8) == 0xF0)  charLen = 4; // 4バイト
        else charLen = 1; // 不正なUTF-8は1バイトとして処理

        result.push_back(str.substr(i, charLen));
        i += charLen;
    }
    return result;
}

// 文字列を逆引きしてバイト配列に変換する関数
std::vector<uint8_t> convertStringToCodes(
    const std::string& text, 
    const std::map<std::string, uint8_t>& reverseMap
) {
    std::vector<uint8_t> result;
    auto chars = splitUTF8(text);

    for (const auto& ch : chars) {
        auto it = reverseMap.find(ch);
        if (it != reverseMap.end()) {
            result.push_back(it->second);
        } else {
            // 見つからなかった場合、0xFFなどで置き換え（未定義コード）
            result.push_back(0xFF);
        }
    }
    return result;
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
    
    // マップ描画
    drawMap();
    bgColor = tft.color565(248, 232, 248); // fontの背景色をポケモンの色パレットに合わせる。
    
    // ポケモン名前取得
    std::vector<uint8_t> pokename = getPokemonName(romPath, dex_id, dex_to_index);
    // ポケモン名前表示
    drawBinaryString(tft, pokename, 170, 32, 2, 2, romPath);
    // ポケモン図鑑番号表示
    std::string str_dex_id = std::to_string(static_cast<unsigned int>(dex_id));
    drawBinaryString(tft, convertStringToCodes(str_dex_id, string2Byte), 170, 2, 2, 2, romPath);

    // スプライト取得
    std::vector<uint8_t> compressed_sprite = getCompressedPokemonSprite(romPath, dex_id, dex_to_index);
    // カラーパレット取得
    std::vector<uint16_t> palette=getPokemonColorPalette(romPath,dex_id);
    // スプライト表示
    displaySpriteImageColor(compressed_sprite,palette.data());

    // 図鑑詳細取得
    std::vector<uint8_t> dex_detail;
    std::vector<uint8_t> poke_information_type;
    std::vector<uint8_t> height_weight;

    //getPokemonDexDetail(romPath, dex_id, dex_to_index, dex_detail, poke_information_type);
    getPokemonDexDetailFull(romPath, dex_id, dex_to_index, dex_detail, poke_information_type,height_weight);
 
    //ポケモンの種族名　〇〇ポケモン
    drawBinaryString(tft, poke_information_type, 182, 78, 2, 1, romPath);
    //文字列からバイト配列に変換し、表示
    std::vector<uint8_t>pokemon_str= convertStringToCodes("ポケモン", string2Byte);
    drawBinaryString(tft, pokemon_str, 218, 78, 2, 1, romPath);

     //ポケモンの高さ
    
    float f = height_weight[0] * 0.1f; // 10で割って小数点1桁にする
    char m[8];
    snprintf(m, sizeof(m), "%.1f", f);

    drawBinaryString(tft, convertStringToCodes(m, string2Byte), 182, 96, 2, 1, romPath);
    // "m"
    drawTileAt(220, 104, 0);


    //ポケモンの重さ
    char kg[8];
      // 2バイトを結合（上位バイト << 8 | 下位バイト）
    uint16_t value = (height_weight[2] << 8) | height_weight[1];
    // 小数点1位までの値に変換（例：0.1単位にスケーリング）
    float fvalue = value * 0.1f;
    snprintf(kg, sizeof(kg), "%.1f", fvalue);

    drawBinaryString(tft, convertStringToCodes(kg, string2Byte), 182, 112, 2, 1, romPath);
    // 描画"kg"
    drawTileAt(220, 120, 1);
    drawTileAt(228, 120, 2);
   
    //ポケモンの図鑑説明
    drawBinaryString(tft, dex_detail, 20, 156, 2, 1, romPath);

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

  // 文字列をバイナリコードに変換して表示
   //逆引きにより、文字列をバイト配列に変換
    string2Byte= BuildChar2ByteTable();


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
    // 0x42784 から 190 バイト読み込み ポケモンのロムインデックスに対応した図鑑番号を取得
    index_to_dex = readROMData(romPath, 0x42784, 190, stopByte);
    // 逆引きテーブル作成
    dex_to_index = buildDexToIndex(index_to_dex);

    // タイルセット構築
    buildTileSet();

    //ポケモン図鑑の初期表示
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
