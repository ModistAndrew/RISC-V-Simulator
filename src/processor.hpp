//
// Created by zjx on 2024/7/29.
//

#ifndef RISC_V_PROCESSOR_HPP
#define RISC_V_PROCESSOR_HPP
#define _DEBUG

#include "instructions.hpp"
using namespace instructions;

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
  Flag pending; // for register 0, data is always 0 and pending is always false.
};

struct PendingData {
  Data data;
  Flag pending; // when pending is true, data stores the position of the pending instruction
};

struct Instruction {
  Flag valid; // set in work()
  Flag ready;
  Opcode opcode;
  std::array<PendingData, 2> pending_data;
  Data immediate; // we always store immediate as signed integer. TODO: consider that in execute()
  RegPos destination;
  Data result; // for branch, result stores whether to jump
  Flag predict; // whether to jump when not ready
  Data pc; // pc of this instruction
  Flag terminate; // for halt instruction
};

struct ProcessorData {
  Data pc;
  std::array<RegisterFile, REGISTER_COUNT> register_files;
  std::array<Instruction, INSTRUCTION_BUFFER_SIZE> instruction_buffer;
  InstPos head, tail;
  Flag flushing;
  Flag committing;
};

// TODO: split out PC, IQ, RS, RoB, SLB, Reg, etc. as separate modules and pass data between them through wires
// TODO: add more modules, e.g. ALU, memory, cache, rather than executing inlined in the processor module
// above may improve clock frequency and make the simulator more realistic

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
    Word code = memory::read_data(to_unsigned(pc));
    OpName op = decode(code);
    if (op == UNKNOWN) {
      code = NO_OPERATION;
      op = ADDI;
    }
    inst.ready.assign(false);
    inst.opcode.assign(op);
    int imm = 0;
    switch (get_op_type(op)) {
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
    for (unsigned int i = 1; i < REGISTER_COUNT; i++) {
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
    auto op = static_cast<OpName>(to_unsigned(inst.opcode));
    if (is_branch(op)) {
      if (inst.result != inst.predict) {
        pc.assign(inst.pc + (inst.result ? to_signed(inst.immediate) : 4));
        flushing.assign(true);
      }
    } else if (is_store(op)) {
      store_data(to_unsigned(inst.pending_data[0].data + inst.immediate), inst.pending_data[1].data,
                 get_memory_access_mode(op));
    } else {
      auto reg_pos = to_unsigned(inst.destination);
      if (reg_pos) {
        RegisterFile &rf = register_files[reg_pos];
        rf.data.assign(inst.result);
        if (rf.pending == true && rf.pending_inst == inst_pos) {
          rf.pending.assign(false);
        }
      }
    }
    if (op == JALR) {
      pc.assign(inst.pending_data[0].data + inst.immediate);
      flushing.assign(true);
    }
    if (inst.terminate == true) {
      should_return.assign(true);
      return_value.assign(to_signed(register_files[10].data));
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
      if (is_store(static_cast<OpName>(to_unsigned(instruction_buffer[inst_pos].opcode)))) {
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
    auto op = static_cast<OpName>(to_unsigned(inst.opcode));
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
        inst.result.assign(rs1 == rs2);
        break;
      case BNE:
        inst.result.assign(rs1 != rs2);
        break;
      case BLT:
        inst.result.assign(rs1 < rs2);
        break;
      case BGE:
        inst.result.assign(rs1 >= rs2);
        break;
      case BLTU:
        inst.result.assign(static_cast<unsigned int>(rs1) < static_cast<unsigned int>(rs2));
        break;
      case BGEU:
        inst.result.assign(static_cast<unsigned int>(rs1) >= static_cast<unsigned int>(rs2));
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

  void work() override {
    if (flushing == true) {
      flush();
      return;
    }
    committing.assign(!committing);
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      Instruction &inst = instruction_buffer[i];
      if (i == tail) {
        if (inst.valid == false && committing == false) {
          fetch(i);
          tail.assign(tail + 1);
          inst.valid.assign(true);
        }
      } else {
        if (inst.valid == true) {
          if (i == head && inst.ready == true) {
            if (committing == true) {
              commit(i);
              head.assign(head + 1);
              inst.valid.assign(false);
            }
          } else if (inst.ready == false) {
            execute(i);
          }
        }
      }
    }
  }
};

#endif //RISC_V_PROCESSOR_HPP
