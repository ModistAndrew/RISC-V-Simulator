//
// Created by zjx on 2024/7/29.
//

#ifndef RISC_V_PROCESSOR_HPP
#define RISC_V_PROCESSOR_HPP

#include <cstddef>
#include "template/module.h"
#include "template/register.h"
#include "template/bit.h"
#include "template/operator.h"

constexpr size_t REGISTER_NUM = 1 << 5;
constexpr size_t INSTRUCTION_BUFFER_SIZE = 1 << 4;
constexpr size_t MEMORY_SIZE = (size_t) 1 << 32;
using InstPos = dark::Register<4>; // a position in instruction buffer.
using PossibleInstPos = dark::Register<5>; // a possible position in instruction buffer. an extra bit for presence.
using Opcode = dark::Register<7>;
using Data = dark::Register<32>; // data or memory address
using RegPos = dark::Register<5>;
using Flag = dark::Register<1>;
using Word = dark::Bit<32>;

std::array<unsigned char, MEMORY_SIZE> memory;

// Load instructions from input stream into our memory.
void load_instructions();

struct ProcessorInput {
  void sync() {
    // do nothing
  }
};

struct ProcessorOutput {
  void sync() {
    // do nothing
  }
};

struct RegisterFile {
  Data data;
  PossibleInstPos pending_inst;
};

enum Op {
  ADD, SUB, MUL, DIV, AND, OR, XOR, SLL, SRL, SRA, SLT, SLTU, ADDI, SLTI, SLTIU, XORI, ORI, ANDI, SLLI, SRLI, SRAI,
  LB, LH, LW, LBU, LHU, SB, SH, SW, BEQ, BNE, BLT, BGE, BLTU, BGEU, JAL, JALR, LUI, AUIPC
};

struct Instruction {
  Flag valid;
  Flag ready;
  Opcode opcode;
  Data data1;
  Data data2;
  PossibleInstPos pending_inst1;
  PossibleInstPos pending_inst2;
  Data immediate;
  RegPos destination;
  Data result;
  Flag predict;
};

struct ProcessorData {
  Data pc;
  std::array<RegisterFile, REGISTER_NUM> register_files;
  std::array<Instruction, INSTRUCTION_BUFFER_SIZE> instruction_buffer;
  InstPos head, tail;
};

// TODO: split out IQ, RS, RoB, SLB, etc. as separate modules and pass data between them through wires
// TODO: add more modules, e.g. ALU, memory, cache, rather than executing inlined in the processor module
// above may reduce clock cycle and make the simulator more realistic

struct ProcessorModule : dark::Module<ProcessorInput, ProcessorOutput, ProcessorData> {
  Word fetch_instruction_code();

  bool decode_instruction(Word code, Instruction &inst);

  void read_instruction_data(Instruction &inst);

  // Fetch an instruction from memory and push it into instruction buffer if there is a vacancy.
  // Fetch from memory according to current pc
  // Decode the instruction. Break if the instruction is invalid
  // Read data from register file or instruction buffer or set pending_inst
  // Push into instruction buffer. Update tail
  // Update pending_inst in register file
  // Predict and update pc
  void fetch(int pos) {
    Instruction inst;
    if (!decode_instruction(fetch_instruction_code(), inst)) {
      return;
    }
    read_instruction_data(inst);
    tail <= tail + 1;
  }

  // Commit the head instruction in instruction buffer if it is ready.
  // For branch inst: if mispredicted, flush instruction buffer and update pc. Or do nothing
  // For store inst: write data to memory
  // For other inst: write result and update pending_inst in register file
  // Pop. Update head
  void commit(int pos);

  // Execute instructions in instruction buffer.
  // Instructions with pending_inst listen to the pending instructions and read data when they are ready
  // Instructions with data ready are executed. Ready bit and result are set
  // Specially, load instructions shouldn't be executed until there is no store instruction uncommitted
  void execute(int pos);

  void work() override {
    for (int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      if(i == tail) {
        if(instruction_buffer[i].valid == false) {
          fetch(i);
          tail <= tail + 1;
        }
      } else if(i == head) {
        commit(i);
      } else {
        execute(i);
      }
    }
  }
};

#endif //RISC_V_PROCESSOR_HPP
