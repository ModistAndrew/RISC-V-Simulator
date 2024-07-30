//
// Created by zjx on 2024/7/29.
//

#ifndef RISC_V_PROCESSOR_HPP
#define RISC_V_PROCESSOR_HPP

#include <cstddef>
#include <string>
#include <iostream>
#include "template/module.h"
#include "template/register.h"
#include "template/bit.h"
#include "template/bit_impl.h"
#include "template/operator.h"
#include "template/tools.h"

constexpr unsigned int REGISTER_NUM = 1 << 5;
constexpr unsigned int INSTRUCTION_BUFFER_SIZE = 1 << 4;
constexpr unsigned int MEMORY_SIZE = 1 << 30;
constexpr unsigned int OPCODE_COUNT = 1 << 7;
using dark::Register, dark::Bit;
using InstPos = Register<4>; // a position in instruction buffer.
using Opcode = Register<7>;
using Data = Register<32>; // data or memory address
using RegPos = Register<5>;
using Flag = Register<1>;
using Return = Register<8>;
using Word = Bit<32>;

std::array<unsigned char, MEMORY_SIZE> memory;

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
    Bit<8> ret;
    ret.set<7, 0>(memory[addr]);
    return mode == BYTE ? to_signed(ret) : to_unsigned(ret);
  }
  if (mode == HALF_WORD || mode == HALF_WORD_UNSIGNED) {
    Bit<16> ret;
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

struct ProcessorInput {
  void sync() {
    // do nothing
  }
};

struct ProcessorOutput {
  Flag should_return;
  Return return_value;
};

struct RegisterFile {
  Data data;
  InstPos pending_inst;
  Flag pending; // for register 0, data is always 0 and pending is always false
};

enum Op {
  LUI,
  AUIPC,
  JAL,
  JALR,
  BEQ,
  BNE,
  BLT,
  BGE,
  BLTU,
  BGEU,
  LB,
  LH,
  LW,
  LBU,
  LHU,
  SB,
  SH,
  SW,
  ADDI,
  SLTI,
  SLTIU,
  XORI,
  ORI,
  ANDI,
  SLLI,
  SRLI,
  SRAI,
  ADD,
  SUB,
  SLL,
  SLT,
  SLTU,
  XOR,
  SRL,
  SRA,
  OR,
  AND,
  UNKNOWN // for invalid instructions
};

Op decode(Word word) {
  auto opcode = to_unsigned(word.range<6, 0>());
  auto funct3 = to_unsigned(word.range<14, 12>());
  auto funct7 = to_unsigned(word.range<31, 25>());
  switch (opcode) {
    case 0b0110111:
      return LUI;
    case 0b0010111:
      return AUIPC;
    case 0b1101111:
      return JAL;
    case 0b1100111:
      return JALR;
    case 0b1100011:
      switch (funct3) {
        case 0b000:
          return BEQ;
        case 0b001:
          return BNE;
        case 0b100:
          return BLT;
        case 0b101:
          return BGE;
        case 0b110:
          return BLTU;
        case 0b111:
          return BGEU;
        default:
          return UNKNOWN;
      }
    case 0b0000011:
      switch (funct3) {
        case 0b000:
          return LB;
        case 0b001:
          return LH;
        case 0b010:
          return LW;
        case 0b100:
          return LBU;
        case 0b101:
          return LHU;
        default:
          return UNKNOWN;
      }
    case 0b0100011:
      switch (funct3) {
        case 0b000:
          return SB;
        case 0b001:
          return SH;
        case 0b010:
          return SW;
        default:
          return UNKNOWN;
      }
    case 0b0010011:
      switch (funct3) {
        case 0b000:
          return ADDI;
        case 0b010:
          return SLTI;
        case 0b011:
          return SLTIU;
        case 0b100:
          return XORI;
        case 0b110:
          return ORI;
        case 0b111:
          return ANDI;
        case 0b001:
          return funct7 == 0b0000000 ? SLLI : UNKNOWN;
        case 0b101:
          return funct7 == 0b0000000 ? SRLI : funct7 == 0b0100000 ? SRAI : UNKNOWN;
        default:
          return UNKNOWN;
      }
    case 0b0110011:
      switch (funct3) {
        case 0b000:
          return funct7 == 0b0000000 ? ADD : funct7 == 0b0100000 ? SUB : UNKNOWN;
        case 0b001:
          return funct7 == 0b0000000 ? SLL : UNKNOWN;
        case 0b010:
          return funct7 == 0b0000000 ? SLT : UNKNOWN;
        case 0b011:
          return funct7 == 0b0000000 ? SLTU : UNKNOWN;
        case 0b100:
          return funct7 == 0b0000000 ? XOR : UNKNOWN;
        case 0b101:
          return funct7 == 0b0000000 ? SRL : funct7 == 0b0100000 ? SRA : UNKNOWN;
        case 0b110:
          return funct7 == 0b0000000 ? OR : UNKNOWN;
        case 0b111:
          return funct7 == 0b0000000 ? AND : UNKNOWN;
        default:
          return UNKNOWN;
      }
    default:
      return UNKNOWN;
  }
}

constexpr Word NO_OPERATION = 0b0010011; // ADDI x0, x0, 0

enum OpType {
  R,
  I1,
  I2,
  S,
  B,
  U,
  J
};

OpType op_type_table[OPCODE_COUNT];

void init() {
  op_type_table[LUI] = U;
  op_type_table[AUIPC] = U;
  op_type_table[JAL] = J;
  op_type_table[JALR] = I1;
  op_type_table[BEQ] = B;
  op_type_table[BNE] = B;
  op_type_table[BLT] = B;
  op_type_table[BGE] = B;
  op_type_table[BLTU] = B;
  op_type_table[BGEU] = B;
  op_type_table[LB] = I1;
  op_type_table[LH] = I1;
  op_type_table[LW] = I1;
  op_type_table[LBU] = I1;
  op_type_table[LHU] = I1;
  op_type_table[SB] = S;
  op_type_table[SH] = S;
  op_type_table[SW] = S;
  op_type_table[ADDI] = I1;
  op_type_table[SLTI] = I1;
  op_type_table[SLTIU] = I1;
  op_type_table[XORI] = I1;
  op_type_table[ORI] = I1;
  op_type_table[ANDI] = I1;
  op_type_table[SLLI] = I2;
  op_type_table[SRLI] = I2;
  op_type_table[SRAI] = I2;
  op_type_table[ADD] = R;
  op_type_table[SUB] = R;
  op_type_table[SLL] = R;
  op_type_table[SLT] = R;
  op_type_table[SLTU] = R;
  op_type_table[XOR] = R;
  op_type_table[SRL] = R;
  op_type_table[SRA] = R;
  op_type_table[OR] = R;
  op_type_table[AND] = R;
}

bool is_branch(Op op) {
  return op == BEQ || op == BNE || op == BLT || op == BGE || op == BLTU || op == BGEU;
}

bool is_store(Op op) {
  return op == SB || op == SH || op == SW;
}

bool is_load(Op op) {
  return op == LB || op == LH || op == LW || op == LBU || op == LHU;
}

MemoryAccessMode get_memory_access_mode(Op op) {
  switch (op) {
    case LB:
    case SB:
      return BYTE;
    case LH:
    case SH:
      return HALF_WORD;
    case LW:
    case SW:
      return WORD;
    case LBU:
      return BYTE_UNSIGNED;
    case LHU:
      return HALF_WORD_UNSIGNED;
    default:
      throw;
  }
}

struct PendingData {
  Data data;
  Flag pending; // when pending is true, data stores the position of the pending instruction
};

struct Instruction {
  Flag valid; // set in work()
  Flag ready;
  Opcode opcode;
  std::array<PendingData, 2> pending_data;
  Data immediate; // we always store immediate as signed integer
  RegPos destination;
  Data result; // for branch, result stores where to jump
  Flag predict; // whether to jump when not ready; whether prediction is correct when ready
  Data pc; // pc of this instruction
  Flag terminate; // for halt instruction
};

struct ProcessorData {
  Data pc;
  std::array<RegisterFile, REGISTER_NUM> register_files;
  std::array<Instruction, INSTRUCTION_BUFFER_SIZE> instruction_buffer;
  InstPos head, tail;
  Flag flushing;
  Flag halt;
};

// TODO: split out IQ, RS, RoB, SLB, etc. as separate modules and pass data between them through wires
// TODO: add more modules, e.g. ALU, memory, cache, rather than executing inlined in the processor module
// above may reduce clock cycle and make the simulator more realistic

struct ProcessorModule : dark::Module<ProcessorInput, ProcessorOutput, ProcessorData> {

  void fill_pending_data(Instruction &inst, unsigned int index, unsigned int reg_pos) {
    if (register_files[reg_pos].pending == false) {
      inst.pending_data[index].pending.assign(false);
      inst.pending_data[index].data.assign(register_files[reg_pos].data);
    } else {
      auto pending_inst_pos = to_unsigned(register_files[reg_pos].pending_inst);
      if (instruction_buffer[pending_inst_pos].ready == true) {
        inst.pending_data[index].pending.assign(false);
        inst.pending_data[index].data.assign(instruction_buffer[pending_inst_pos].result);
      } else {
        inst.pending_data[index].pending.assign(true);
        inst.pending_data[index].data.assign(pending_inst_pos);
      }
    }
  }

  void set_destination(Instruction &inst, unsigned int reg_pos, unsigned int inst_pos) {
    inst.destination.assign(reg_pos);
    if (reg_pos) { // x0 is always 0
      register_files[reg_pos].pending_inst.assign(inst_pos);
      register_files[reg_pos].pending.assign(true);
    }
  }

  // Fetch an instruction from memory and push it into instruction buffer.
  // Fetch from memory according to current pc
  // Decode the instruction
  // Read data from register file or instruction buffer or set pending_inst
  // Update pending_inst in register file
  // Predict and update pc
  void fetch(unsigned int inst_pos) {
    Instruction &inst = instruction_buffer[inst_pos];
    Word code = read_data(to_unsigned(pc));
    Op op = decode(code);
    if (op == UNKNOWN) {
      code = NO_OPERATION;
      op = ADDI;
    }
    inst.ready.assign(false);
    inst.opcode.assign(op);
    int imm = 0;
    switch (op_type_table[op]) {
      case R:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        fill_pending_data(inst, 1, to_unsigned(code.range<24, 20>()));
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos);
        break;
      case I1:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        inst.pending_data[1].pending.assign(false); // not used
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos);
        imm = to_signed(code.range<31, 20>());
        break;
      case I2:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        inst.pending_data[1].pending.assign(false);
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos);
        imm = to_signed(code.range<24, 20>());
        break;
      case S:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        fill_pending_data(inst, 1, to_unsigned(code.range<24, 20>()));
        imm = to_signed(Bit(code.range<31, 25>(), code.range<11, 7>()));
        break;
      case B:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        fill_pending_data(inst, 1, to_unsigned(code.range<24, 20>()));
        imm = to_signed(
          Bit(code.range<31, 31>(), code.range<7, 7>(), code.range<30, 25>(), code.range<11, 8>(), Bit<1>()));
        break;
      case U:
        inst.pending_data[0].pending.assign(false);
        inst.pending_data[1].pending.assign(false);
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos);
        imm = to_signed(Bit(code.range<31, 12>(), Bit<12>()));
        break;
      case J:
        inst.pending_data[0].pending.assign(false);
        inst.pending_data[1].pending.assign(false);
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos);
        imm = to_signed(
          Bit(code.range<31, 31>(), code.range<19, 12>(), code.range<20, 20>(), code.range<30, 21>(), Bit<1>()));
        break;
    }
    inst.immediate.assign(imm);
    inst.pc.assign(pc);
    if (is_branch(op)) {
      inst.predict.assign(true);
      pc.assign(pc + imm);
    } else if (op == JAL) {
      pc.assign(pc + imm); // always jump
    } else { // JALR should be handled when committed
      pc.assign(pc + 4);
    }
    if (code == 0x0ff00513) {
      inst.terminate.assign(true);
    }
  }

  void flush() {
    head.assign(0);
    tail.assign(0);
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      instruction_buffer[i].valid.assign(false);
    }
    for (unsigned int i = 1; i < REGISTER_NUM; i++) {
      register_files[i].pending.assign(false);
    }
    flushing.assign(false);
  }

  // Commit the head instruction in instruction buffer.
  // For branch inst: if mispredicted, flush instruction buffer and update pc. Or do nothing
  // For store inst: write data to memory
  // For other inst: write result and update pending_inst in register file
  // return true if flush is needed
  void commit(unsigned int inst_pos) {
    Instruction &inst = instruction_buffer[inst_pos];
    Op op = static_cast<Op>(to_unsigned(inst.opcode));
    if (is_branch(op)) {
      if (inst.predict == false) {
        pc.assign(inst.result);
        flushing.assign(true);
      }
    } else if (is_store(op)) {
      store_data(to_unsigned(inst.pending_data[0].data + inst.immediate), inst.pending_data[1].data,
                 get_memory_access_mode(op));
    } else {
      RegisterFile &rf = register_files[to_unsigned(inst.destination)];
      rf.data.assign(inst.result);
      if (rf.pending == true && rf.pending_inst == inst_pos) {
        rf.pending.assign(false);
      }
    }
    if (op == JALR) {
      pc.assign(inst.pending_data[0].data + inst.immediate);
      flushing.assign(true);
    }
    if (inst.terminate == true) {
      halt.assign(true);
    }
  }

  // return true if pending
  void ask_for_data(Instruction &inst, unsigned int index) {
    PendingData &pending_data = inst.pending_data[index];
    if (pending_data.pending == true) {
      Instruction &pending_inst = instruction_buffer[to_unsigned(pending_data.data)];
      if (pending_inst.ready == true) {
        pending_data.pending.assign(false);
        pending_data.data.assign(pending_inst.result);
      }
    }
  }

  bool has_uncommitted_store(unsigned int inst_pos) {
    while (inst_pos != head) {
      if (inst_pos == 0) {
        inst_pos = INSTRUCTION_BUFFER_SIZE - 1;
      } else {
        inst_pos--;
      }
      if (instruction_buffer[inst_pos].valid == false) {
        throw; // should not happen
      }
      if (is_store(static_cast<Op>(to_unsigned(instruction_buffer[inst_pos].opcode)))) {
        return true;
      }
    }
    return false;
  }

  // Execute instructions in instruction buffer.
  // Instructions with pending_inst listen to the pending instructions and read data when they are ready
  // Instructions with data ready are executed. Ready bit and result are set
  // Specially, load instructions shouldn't be executed until there is no store instruction uncommitted
  void execute(unsigned int inst_pos) {
    Instruction &inst = instruction_buffer[inst_pos];
    ask_for_data(inst, 0);
    ask_for_data(inst, 1);
    if (inst.pending_data[0].pending == true || inst.pending_data[1].pending == true) {
      return;
    }
    Op op = static_cast<Op>(to_unsigned(inst.opcode));
    if (is_load(op) && has_uncommitted_store(inst_pos)) {
      return;
    }
    inst.ready.assign(true);
    auto rs1 = to_signed(inst.pending_data[0].data);
    auto rs2 = to_signed(inst.pending_data[1].data);
    auto imm = to_signed(inst.immediate);
    auto pc = to_unsigned(inst.pc);
    switch (op) {
      case LUI:
        inst.result.assign(imm);
        break;
      case AUIPC:
        inst.result.assign(pc + imm);
        break;
      case JAL:
      case JALR:
        inst.result.assign(pc + 4);
        break;
      case BEQ:
        inst.result.assign(rs1 == rs2 ? pc + imm : pc + 4);
        break;
      case BNE:
        inst.result.assign(rs1 != rs2 ? pc + imm : pc + 4);
        break;
      case BLT:
        inst.result.assign(rs1 < rs2 ? pc + imm : pc + 4);
        break;
      case BGE:
        inst.result.assign(rs1 >= rs2 ? pc + imm : pc + 4);
        break;
      case BLTU:
        inst.result.assign(static_cast<unsigned int>(rs1) < static_cast<unsigned int>(rs2) ? pc + imm : pc + 4);
        break;
      case BGEU:
        inst.result.assign(static_cast<unsigned int>(rs1) >= static_cast<unsigned int>(rs2) ? pc + imm : pc + 4);
        break;
      case LB:
      case LH:
      case LW:
      case LBU:
      case LHU:
        inst.result.assign(read_data(rs1 + imm, get_memory_access_mode(op)));
        break;
      case SB:
      case SH:
      case SW:
        break; // three store instructions which have no result
      case ADDI:
        inst.result.assign(rs1 + imm);
        break;
      case SLTI:
        inst.result.assign(rs1 < imm ? 1 : 0);
        break;
      case SLTIU:
        inst.result.assign(static_cast<unsigned int>(rs1) < static_cast<unsigned int>(imm) ? 1 : 0);
        break;
      case XORI:
        inst.result.assign(rs1 ^ imm);
        break;
      case ORI:
        inst.result.assign(rs1 | imm);
        break;
      case ANDI:
        inst.result.assign(rs1 & imm);
        break;
      case SLLI:
        inst.result.assign(rs1 << imm);
        break;
      case SRLI:
        inst.result.assign(static_cast<unsigned int>(rs1) >> imm);
        break;
      case SRAI:
        inst.result.assign(rs1 >> imm);
        break;
      case ADD:
        inst.result.assign(rs1 + rs2);
        break;
      case SUB:
        inst.result.assign(rs1 - rs2);
        break;
      case SLL:
        inst.result.assign(rs1 << (rs2 & 0b11111));
        break;
      case SLT:
        inst.result.assign(rs1 < rs2 ? 1 : 0);
        break;
      case SLTU:
        inst.result.assign(static_cast<unsigned int>(rs1) < static_cast<unsigned int>(rs2) ? 1 : 0);
        break;
      case XOR:
        inst.result.assign(rs1 ^ rs2);
        break;
      case SRL:
        inst.result.assign(static_cast<unsigned int>(rs1) >> (rs2 & 0b11111));
        break;
      case SRA:
        inst.result.assign(rs1 >> (rs2 & 0b11111));
        break;
      case OR:
        inst.result.assign(rs1 | rs2);
        break;
      case AND:
        inst.result.assign(rs1 & rs2);
        break;
      default:
        throw;
    }
  }

  void work()
  override {
    if (halt == true) {
      should_return.assign(true);
      return_value.assign(to_signed(register_files[10].data));
      return;
    }
    if (flushing == true) {
      flush();
      return;
    }
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      Instruction &inst = instruction_buffer[i];
      if (i == tail) {
        if (inst.valid == false) {
          fetch(i);
          tail.assign(tail + 1);
          inst.valid.assign(true);
        }
      } else if (i == head) {
        if (inst.valid == true && inst.ready == true) {
          commit(i);
          head.assign(head + 1);
          inst.valid.assign(false);
        }
      } else {
        if (inst.valid == true && inst.ready == false) {
          execute(i);
        }
      }
    }
  }
};

#endif //RISC_V_PROCESSOR_HPP
