//
// Created by zjx on 2024/7/31.
//

#ifndef RISC_V_MEMORY_HPP
#define RISC_V_MEMORY_HPP

#include <array>
#include <iostream>
#include "constants.hpp"

namespace memory {
  std::unordered_map<unsigned int, unsigned char> memory;

  // Load instructions from input stream into our memory.
  void load_instructions() {
    unsigned int current_pos = 0;
    std::string str;
    while (std::cin >> str) {
      if (str[0] == '@') {
        current_pos = std::stoul(str.substr(1), nullptr, 16);
      } else {
        memory[current_pos] = std::stoul(str, nullptr, 16);
        current_pos++;
      }
    }
  }

  enum MemoryAccessMode {
    BYTE,
    BYTE_UNSIGNED,
    HALF_WORD,
    HALF_WORD_UNSIGNED,
    WORD
  };

  Word read_data(unsigned int addr, MemoryAccessMode mode = WORD) {
    if (mode == BYTE || mode == BYTE_UNSIGNED) {
      Byte ret;
      ret.set<7, 0>(memory[addr]);
      return mode == BYTE ? to_signed(ret) : to_unsigned(ret);
    }
    if (mode == HALF_WORD || mode == HALF_WORD_UNSIGNED) {
      HalfWord ret;
      ret.set<7, 0>(memory[addr]);
      ret.set<15, 8>(memory[addr + 1]);
      return mode == HALF_WORD ? to_signed(ret) : to_unsigned(ret);
    }
    Word ret;
    ret.set<7, 0>(memory[addr]);
    ret.set<15, 8>(memory[addr + 1]);
    ret.set<23, 16>(memory[addr + 2]);
    ret.set<31, 24>(memory[addr + 3]);
    return ret;
  }

  void store_data(unsigned int addr, const Word &data, MemoryAccessMode mode = WORD) {
    memory[addr] = to_unsigned(data.range<7, 0>());
    if (mode == BYTE || mode == BYTE_UNSIGNED) {
      return;
    }
    memory[addr + 1] = to_unsigned(data.range<15, 8>());
    if (mode == HALF_WORD || mode == HALF_WORD_UNSIGNED) {
      return;
    }
    memory[addr + 2] = to_unsigned(data.range<23, 16>());
    memory[addr + 3] = to_unsigned(data.range<31, 24>());
  }
}

#endif //RISC_V_MEMORY_HPP