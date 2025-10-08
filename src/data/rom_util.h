#pragma once
#include <Arduino.h>
#include<vector>

std::vector<uint8_t> readROMData(const std::string &path, uint32_t startAddr, size_t maxLength, const std::vector<uint8_t>& stopSequence = {});
uint16_t readLittleEndian16(const std::vector<uint8_t>& data, size_t offset);