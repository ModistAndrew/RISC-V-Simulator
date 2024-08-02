//
// Created by zjx on 2024/7/29.
//

#ifndef RISC_V_PROCESSOR_HPP
#define RISC_V_PROCESSOR_HPP
#define _DEBUG

#include "instructions.hpp"

using namespace instructions;

struct ProcessorInput {
  FlagWire memory_busy;
  FlagWire memory_load_finished;
  FlagWire memory_store_finished;
  DataWire memory_data;
};

struct ProcessorOutput {
  Flag should_return;
  Return return_value;
  Flag load;
  Flag store;
  Data addr;
  MemoryAccessModeCode memory_mode; // the mode of the load instruction
  Data store_data;
  Flag flushing;
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
  OpCode opcode;
  std::array<PendingData, 2> pending_data;
  Data immediate; // we always store immediate as signed integer. TODO: consider that in execute()
  RegPos destination;
  Data result; // for branch, result stores whether to jump
  Flag predict; // whether to jump when not ready
  Data pc; // pc of this instruction
  Flag terminate; // for halt instruction
};

enum PredictorStatus {
  STRONGLY_NOT_TAKEN,
  WEAKLY_NOT_TAKEN,
  WEAKLY_TAKEN,
  STRONGLY_TAKEN
};

struct ProcessorData {
  Data pc;
  std::array<RegisterFile, REGISTER_COUNT> register_files;
  std::array<Instruction, INSTRUCTION_BUFFER_SIZE> instruction_buffer;
  std::array<PredictorStatusCode, PREDICTOR_HASH_SIZE> predictors;
  InstPos head, tail;
  Data flush_pc; // pc to flush to
  InstPos mem_inst_pos; // the position of the load instruction
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

  void set_destination(Instruction &inst, unsigned int reg_pos, unsigned int inst_pos, unsigned int &dest) {
    inst.destination.assign(reg_pos);
    if (reg_pos) { // x0 is always 0
      register_files[reg_pos].pending_inst.assign(inst_pos);
      register_files[reg_pos].pending.assign(true);
      dest = reg_pos;
    }
  }

  bool get_predict(unsigned int pc) {
    auto hash = pc & (PREDICTOR_HASH_SIZE - 1);
    auto &pr = predictors[hash];
    auto state = static_cast<PredictorStatus>(to_unsigned(pr));
    return state == STRONGLY_TAKEN || state == WEAKLY_TAKEN;
  }

  void store_predict(unsigned int pc, bool result) {
    auto hash = pc & (PREDICTOR_HASH_SIZE - 1);
    auto &pr = predictors[hash];
    auto state = static_cast<PredictorStatus>(to_unsigned(pr));
    switch (state) {
      case STRONGLY_NOT_TAKEN:
        pr.assign(result ? WEAKLY_NOT_TAKEN : STRONGLY_NOT_TAKEN);
        break;
      case WEAKLY_NOT_TAKEN:
        pr.assign(result ? WEAKLY_TAKEN : STRONGLY_NOT_TAKEN);
        break;
      case WEAKLY_TAKEN:
        pr.assign(result ? STRONGLY_TAKEN : WEAKLY_NOT_TAKEN);
        break;
      case STRONGLY_TAKEN:
        pr.assign(result ? STRONGLY_TAKEN : WEAKLY_TAKEN);
        break;
    }
  }

  // Fetch an instruction from memory and push it into instruction buffer.
  // Fetch from memory according to current pc
  // Decode the instruction
  // Read data from register file or instruction buffer or set pending_inst
  // Update pending_inst in register file
  // Predict and update pc
  // may modify dest
  void fetch(unsigned int &dest) {
    auto inst_pos = to_unsigned(tail);
    Instruction &inst = instruction_buffer[inst_pos];
    if (inst.valid == true) {
      return;
    }
    Word code = memory::load_data(to_unsigned(pc));
    Op op = decode(code);
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
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos, dest);
        break;
      case I1:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        inst.pending_data[1].pending.assign(false); // not used
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos, dest);
        imm = to_signed(code.range<31, 20>());
        break;
      case I2:
        fill_pending_data(inst, 0, to_unsigned(code.range<19, 15>()));
        inst.pending_data[1].pending.assign(false);
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos, dest);
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
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos, dest);
        imm = to_signed(Bit(code.range<31, 12>(), Bit<12>()));
        break;
      case J:
        inst.pending_data[0].pending.assign(false);
        inst.pending_data[1].pending.assign(false);
        set_destination(inst, to_unsigned(code.range<11, 7>()), inst_pos, dest);
        imm = to_signed(
          Bit(code.range<31, 31>(), code.range<19, 12>(), code.range<20, 20>(), code.range<30, 21>(), Bit<1>()));
        break;
    }
    inst.immediate.assign(imm);
    inst.pc.assign(pc);
    if (is_branch(op)) {
      bool predict = get_predict(to_unsigned(pc));
      inst.predict.assign(predict);
      pc.assign(pc + (predict ? imm : 4));
    } else if (op == JAL) {
      pc.assign(pc + imm); // always jump
    } else { // JALR should be handled when committed
      pc.assign(pc + 4);
    }
    if (code == 0x0ff00513) {
      inst.terminate.assign(true);
    }
    tail.assign(tail + 1);
    inst.valid.assign(true);
  }

  void flush() {
    head.assign(0);
    tail.assign(0);
    pc.assign(flush_pc);
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      instruction_buffer[i].valid.assign(false);
    }
    for (unsigned int i = 1; i < REGISTER_COUNT; i++) {
      register_files[i].pending.assign(false);
    }
    load.assign(false);
    store.assign(false);
    flushing.assign(false);
  }

  // Commit the head instruction in instruction buffer.
  // For branch inst: if mispredicted, flush instruction buffer and update pc. Or do nothing
  // For store inst: write data to memory
  // For other inst: write result and update pending_inst in register file
  // return true if flush is needed
  void commit(unsigned int dest) {
    auto inst_pos = to_unsigned(head);
    Instruction &inst = instruction_buffer[inst_pos];
    if (inst.valid == false || inst.ready == false) {
      return;
    }
    auto op = static_cast<Op>(to_unsigned(inst.opcode));
    if (is_branch(op)) {
      total_predict++;
      auto result = static_cast<bool>(inst.result);
      if (result != inst.predict) {
        store_predict(to_unsigned(inst.pc), result);
        flush_pc.assign(inst.pc + (result ? to_signed(inst.immediate) : 4));
        flushing.assign(true);
      } else {
        correct_predict++;
      }
    } else if (is_store(op)) {
      if (memory_busy == false) {
        store.assign(true);
        addr.assign(to_unsigned(inst.pending_data[0].data + inst.immediate));
        store_data.assign(inst.pending_data[1].data);
        memory_mode.assign(get_memory_access_mode(op));
        mem_inst_pos.assign(inst_pos);
      }
      return;
    } else {
      auto reg_pos = to_unsigned(inst.destination);
      if (reg_pos) {
        register_files[reg_pos].data.assign(inst.result);
        if (reg_pos != dest && register_files[reg_pos].pending == true &&
            register_files[reg_pos].pending_inst == inst_pos) {
          register_files[reg_pos].pending.assign(false);
        }
      }
    }
    if (op == JALR) {
      flush_pc.assign(inst.pending_data[0].data + inst.immediate);
      flushing.assign(true);
    }
    if (inst.terminate == true) {
      should_return.assign(true);
      return_value.assign(to_signed(register_files[10].data));
    }
    head.assign(head + 1);
    inst.valid.assign(false);
    total_committed++;
  }

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

  void read_data() {
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      Instruction &inst = instruction_buffer[i];
      if (inst.valid == true && inst.ready == false) {
        ask_for_data(inst, 0);
        ask_for_data(inst, 1);
      }
    }
  }

  void execute_load() {
    if (memory_busy) {
      return;
    }
    auto current_inst_pos = to_unsigned(head);
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      Instruction &inst = instruction_buffer[current_inst_pos];
      auto op = static_cast<Op>(to_unsigned(inst.opcode));
      if (inst.valid == true) {
        if (is_store(op)) {
          return;
        }
        if (is_load(op) && inst.ready == false &&
            inst.pending_data[0].pending == false && inst.pending_data[1].pending == false) {
          auto rs1 = to_signed(inst.pending_data[0].data);
          auto imm = to_signed(inst.immediate);
          mem_inst_pos.assign(current_inst_pos);
          load.assign(true);
          addr.assign(rs1 + imm);
          memory_mode.assign(get_memory_access_mode(op));
          return;
        }
        current_inst_pos++;
        if (current_inst_pos == INSTRUCTION_BUFFER_SIZE) {
          current_inst_pos = 0;
        }
        continue;
      }
      break;
    }
  }

// Execute instructions in instruction buffer.
// Instructions with pending_inst listen to the pending instructions and read data when they are ready
// Instructions with data ready are executed. Ready bit and result are set
// Specially, load instructions shouldn't be executed until there is no store instruction uncommitted
  void execute_alu() {
    for (unsigned int i = 0; i < INSTRUCTION_BUFFER_SIZE; i++) {
      Instruction &inst = instruction_buffer[i];
      auto op = static_cast<Op>(to_unsigned(inst.opcode));
      if (inst.valid == true && inst.ready == false &&
          inst.pending_data[0].pending == false && inst.pending_data[1].pending == false
          && !is_load(op)) {
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
        inst.ready.assign(true);
        return;
      }
    }
  }

  void work() override {
    if (flushing) {
      flush();
      return;
    }
    if (memory_load_finished) {
      Instruction &load_inst = instruction_buffer[to_unsigned(mem_inst_pos)];
      load_inst.result.assign(memory_data);
      load_inst.ready.assign(true);
      load.assign(false);
    }
    if (memory_store_finished) {
      Instruction &store_inst = instruction_buffer[to_unsigned(mem_inst_pos)];
      head.assign(head + 1);
      store_inst.valid.assign(false);
      store.assign(false);
    }
    unsigned int dest = -1;
    fetch(dest);
    commit(dest);
    read_data();
    execute_alu();
    execute_load();
  }
};

#endif //RISC_V_PROCESSOR_HPP
