//
// Created by zjx on 2024/7/30.
//
#include <iostream>
#include "processor.hpp"

void load_instructions() {
  size_t current_pos = 0;
  std::string str;
  while(std::cin >> str) {
    if(str[0] == '@') {
      current_pos = std::stoul(str.substr(1), nullptr, 16);
    } else {
      memory[current_pos] = std::stoul(str, nullptr, 16);
      current_pos++;
    }
  }
}