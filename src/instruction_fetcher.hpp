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
  Data immediate; // we always store immediate as signed integer.
  RegPos destination;
  Data pc; // pc of this instruction
  Flag terminate; // for halt instruction
  Flag predict; // whether to jump when not ready
};

struct InstructionFetcher : public dark::Module<InstructionFetcherInput, InstructionFetcherOutput> {
  void work() override {
    Word code = memory::read_data(to_unsigned(current_pc));
    OpName op = decode(code);
    if (op == UNKNOWN) {
      code = NO_OPERATION;
      op = ADDI;
    }
    opcode.assign(op);
    source[0].assign(to_unsigned(code.range<19, 15>()));
    source[1].assign(to_unsigned(code.range<24, 20>()));
    switch (get_op_type(op)) {
      case R:
        break;
      case I1:
        immediate.assign(to_signed(code.range<31, 20>()));
        break;
      case I2:
        immediate.assign(to_signed(code.range<24, 20>()));
        break;
      case S:
        immediate.assign(to_signed(Bit(code.range<31, 25>(), code.range<11, 7>())));
        break;
      case U:
        immediate.assign(to_signed(Bit(code.range<31, 12>(), Bit<12>())));
        break;
      case B:
        immediate.assign(to_signed(
          Bit(code.range<31, 31>(), code.range<7, 7>(), code.range<30, 25>(), code.range<11, 8>(), Bit<1>())));
        break;
      case J:
        immediate.assign(to_signed(
          Bit(code.range<31, 31>(), code.range<19, 12>(), code.range<20, 20>(), code.range<30, 21>(), Bit<1>())));
        break;
    }
    destination.assign(to_unsigned(code.range<11, 7>()));
    pc.assign(current_pc);
    terminate.assign(code == TERMINATION);
    if (is_branch(op)) {
      inst.predict.assign(true);
      pc.assign(pc + imm);
    } else if (op == JAL) {
      pc.assign(pc + imm); // always jump
    } else { // JALR should be handled when committed
      pc.assign(pc + 4);
    }

  }
};

#endif //RISC_V_INSTRUCTION_FETCHER_HPP
