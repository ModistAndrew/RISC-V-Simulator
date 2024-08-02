//
// Created by zjx on 2024/8/1.
//

#ifndef RISC_V_PROGRAM_COUNTER_HPP
#define RISC_V_PROGRAM_COUNTER_HPP

#include "constants.hpp"

struct ProgramCounterInput {
  FlagWire predict;
  DataWire next_pc;
};

struct ProgramCounterOutput {
  Data pc;
};

struct ProgramCounter : public dark::Module<ProgramCounterInput, ProgramCounterOutput> {
  void work() override {
    pc.assign(predict == true ? next_pc : pc + 4);
  }
};

#endif //RISC_V_PROGRAM_COUNTER_HPP
