//
// Created by zjx on 2024/8/1.
//

#ifndef RISC_V_INSTRUCTION_FETCHER_HPP
#define RISC_V_INSTRUCTION_FETCHER_HPP

#include "instructions.hpp"

using namespace instructions;

struct InstructionFetcherInput {
  DataWire current_pc;
};

struct InstructionFetcherOutput {
  Opcode opcode;
  std::array<RegPos, 2> source;
  std::array<Flag, 2> source_present;
  Data immediate; // we always store immediate as signed integer.
  RegPos destination;
  Data pc; // pc of this instruction
  Flag terminate; // for halt instruction
  Flag predict; // whether to jump when not ready
  Data next_pc; // pc of next instruction
};

struct InstructionFetcher : public dark::Module<InstructionFetcherInput, InstructionFetcherOutput> {
  void work() override {
    Word code = memory::read_data(to_unsigned(current_pc)); // read instantly
    OpName op = decode(code);
    opcode.assign(op);
    source[0].assign(to_unsigned(code.range<19, 15>()));
    source[1].assign(to_unsigned(code.range<24, 20>()));
    int imm;
    switch (get_op_type(op)) {
      case R:
        source_present[0].assign(true);
        source_present[1].assign(true);
        imm = 0;
        break;
      case I1:
        source_present[0].assign(true);
        source_present[1].assign(false);
        imm = to_signed(code.range<31, 20>());
        break;
      case I2:
        source_present[0].assign(true);
        source_present[1].assign(false);
        imm = to_signed(code.range<24, 20>());
        break;
      case S:
        source_present[0].assign(true);
        source_present[1].assign(true);
        imm = to_signed(Bit(code.range<31, 25>(), code.range<11, 7>()));
        break;
      case U:
        source_present[0].assign(false);
        source_present[1].assign(false);
        imm = to_signed(Bit(code.range<31, 12>(), Bit<12>()));
        break;
      case B:
        source_present[0].assign(true);
        source_present[1].assign(true);
        imm = to_signed(
          Bit(code.range<31, 31>(), code.range<7, 7>(), code.range<30, 25>(), code.range<11, 8>(), Bit<1>()));
        break;
      case J:
        source_present[0].assign(false);
        source_present[1].assign(false);
        imm = to_signed(
          Bit(code.range<31, 31>(), code.range<19, 12>(), code.range<20, 20>(), code.range<30, 21>(), Bit<1>()));
        break;
    }
    immediate.assign(imm);
    destination.assign(to_unsigned(code.range<11, 7>()));
    pc.assign(current_pc);
    terminate.assign(code == TERMINATION);
    predict.assign(is_branch(op));
    next_pc.assign(is_branch(op) || op == JAL ? current_pc + imm : current_pc + 4);
  }
};

#endif //RISC_V_INSTRUCTION_FETCHER_HPP
