#include <Arduino.h>
#include <LittleFS.h>
#include <vector>
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
