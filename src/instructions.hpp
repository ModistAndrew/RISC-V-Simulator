//
// Created by zjx on 2024/7/31.
//

#ifndef RISC_V_INSTRUCTIONS_HPP
#define RISC_V_INSTRUCTIONS_HPP

#include "memory.hpp"

namespace instructions {
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

  enum OpType {
    R,
    I1,
    I2,
    S,
    B,
    U,
    J
  };

  constexpr Word NO_OPERATION = 0b0010011; // ADDI x0, x0, 0
  constexpr Word TERMINATION = 0x0ff00513;

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

  OpType get_op_type(Op op) {
    switch (op) {
      case LUI:
      case AUIPC:
        return U;
      case JAL:
        return J;
      case JALR:
      case LB:
      case LH:
      case LW:
      case LBU:
      case LHU:
      case ADDI:
      case SLTI:
      case SLTIU:
      case XORI:
      case ORI:
      case ANDI:
        return I1;
      case SLLI:
      case SRLI:
      case SRAI:
        return I2;
      case BEQ:
      case BNE:
      case BLT:
      case BGE:
      case BLTU:
      case BGEU:
        return B;
      case SB:
      case SH:
      case SW:
        return S;
      case ADD:
      case SUB:
      case SLL:
      case SLT:
      case SLTU:
      case XOR:
      case SRL:
      case SRA:
      case OR:
      case AND:
        return R;
      default:
        throw std::invalid_argument("Invalid instruction type.");
    }
  }

  bool is_branch(Op op) {
    switch (op) {
      case BEQ:
      case BNE:
      case BLT:
      case BGE:
      case BLTU:
      case BGEU:
        return true;
      default:
        return false;
    }
  }

  bool is_load(Op op) {
    switch (op) {
      case LB:
      case LH:
      case LW:
      case LBU:
      case LHU:
        return true;
      default:
        return false;
    }
  }

  bool is_store(Op op) {
    switch (op) {
      case SB:
      case SH:
      case SW:
        return true;
      default:
        return false;
    }
  }

  memory::MemoryAccessMode get_memory_access_mode(Op op) {
    switch (op) {
      case LB:
      case SB:
        return memory::BYTE;
      case LH:
      case SH:
        return memory::HALF_WORD;
      case LW:
      case SW:
        return memory::WORD;
      case LBU:
        return memory::BYTE_UNSIGNED;
      case LHU:
        return memory::HALF_WORD_UNSIGNED;
      default:
        throw std::invalid_argument("Invalid memory access mode.");
    }
  }
}
#endif //RISC_V_INSTRUCTIONS_HPP
