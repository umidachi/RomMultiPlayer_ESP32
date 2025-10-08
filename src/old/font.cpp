#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include "font_table.h"

TFT_eSPI tft = TFT_eSPI();

// ==== 8x8 1bppフォント描画 ====
void drawFont8x8(int x, int y, const uint8_t* data, uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t scale = 2) {
    for (int row = 0; row < 8; row++) {
        uint8_t line = data[row];
        for (int col = 0; col < 8; col++) {
            uint16_t px = (line & (0x80 >> col)) ? color : bg;
            tft.fillRect(x + col * scale, y + row * scale, scale, scale, px);
        }
    }
}

// ==== 上文字+ベース文字を積み上げて描画 ====
void drawKanaStacked(uint8_t code, int x, int y, uint16_t color = TFT_WHITE, uint16_t bg = TFT_BLACK, uint8_t scale = 2) {
    auto it = fontTable.find(code);
    if (it == fontTable.end()) {
        Serial.printf("未定義コード %02X\n", code);
        return;
    }

    FontInfo info = it->second;
    uint8_t buf[8];

    File rom = LittleFS.open("/pokemon_blue.gb", "r");
    if (!rom) {
        Serial.println("ROM読み込み失敗");
        return;
    }

    // --- 上文字描画（あれば） ---
    if (info.accentAddress != 0) {
        rom.seek(info.accentAddress, SeekSet);
        if (rom.read(buf, 8) != 8) {
            Serial.printf("ROM読み込み失敗 (accent) code=%02X\n", code);
        } else {
            drawFont8x8(x, y, buf, color, bg, scale); // 上文字は上半分
        }
    }

    // --- ベース文字描画 ---
    rom.seek(info.baseAddress, SeekSet);
    if (rom.read(buf, 8) != 8) {
        Serial.printf("ROM読み込み失敗 (base) code=%02X\n", code);
    } else {
        drawFont8x8(x, y + 8 * scale, buf, color, bg, scale); // ベース文字は下半分
    }

    rom.close();
    Serial.printf("表示: %s (code=%02X)\n", info.character, code);
}

// ==== 配列バイト列を順に表示 ====
void drawBinaryString(const uint8_t* data, size_t len, int startX, int startY, int spacing = 2, uint8_t scale = 2) {
    int x = startX;
    int y = startY;

    for (size_t i = 0; i < len; i++) {
        uint8_t code = data[i];

        // 改行処理
        if (code == 0x4E || code == 0x4F) { // 0x4E:改行, 0x4F:改行(2行分)
            x = startX;
            y += 16 * scale + spacing;
            continue;
        }

        // スペース処理
        if (code == 0x7F) {
            x += 8 * scale + spacing;
            continue;
        }

        drawKanaStacked(code, x, y, TFT_WHITE, TFT_BLACK, scale);
        x += 8 * scale + spacing;

        // 画面幅超えたら改行
        if (x + 8 * scale > tft.width()) {
            x = startX;
            y += 16 * scale + spacing;
        }
    }
}
// ==== セットアップ ====
void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS初期化失敗");
        return;
    }

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // 表示するバイナリ配列例
    //const uint8_t message[] = {0x05, 0x76, 0xF7, 0xE7}; // 「ガ」「パ」「あ」「い」
    //const uint8_t message[] = {0x82, 0x91, 0x1c,0xac,0x93};
    const uint8_t message[] = {0x82, 0x4F,0x91, 0x7F, 0x1c,0xac,0x4E,0x93};

    drawBinaryString(message, sizeof(message), 20, 20, 2, 2);
}

// ==== ループ ====
void loop() {
    // 静的表示なので何もしない
}
