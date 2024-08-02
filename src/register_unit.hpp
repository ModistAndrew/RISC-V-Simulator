//
// Created by zjx on 2024/7/31.
//

#ifndef RISC_V_REGISTER_FILE_HPP
#define RISC_V_REGISTER_FILE_HPP

#include "constants.hpp"

struct RegisterFile {
  Data data;
  InstPos pending_inst;
  Flag pending; // for register 0, data is always 0 and pending is always false.
};

struct RegisterUnitInput {
  std::array<DataWire, INSTRUCTION_BUFFER_SIZE> instruction_buffer_result;
  std::array<FlagWire, INSTRUCTION_BUFFER_SIZE> instruction_buffer_ready;
  std::array<RegPosWire, 2> fetch_source_reg;
};

struct RegisterUnitOutput {
  std::array<PendingData, 2> fetch_source_pending;
  std::array<DataWire, 2> fetch_source_data;
};

struct RegisterUnitData {
  std::array<RegisterFile, REGISTER_COUNT> register_files;
};

struct RegisterUnit : public dark::Module<RegisterUnitInput, RegisterUnitOutput, RegisterUnitData> {
  void work() override {
    for (int i = 0; i < 2; i++) {
      if (register_files[to_unsigned(fetch_source_reg[i])].pending == false) {
        fetch_source_output[i].pending.assign(false);
        fetch_source_output[i].data.assign(register_files[to_unsigned(fetch_source_reg[i])].data);
      } else {
        auto pending_inst_pos = to_unsigned(register_files[to_unsigned(fetch_source_reg[i])].pending_inst);
        if (instruction_buffer_ready[pending_inst_pos] == true) {
          fetch_source_output[i].pending.assign(false);
          fetch_source_output[i].data.assign(instruction_buffer_result[pending_inst_pos]);
        } else {
          fetch_source_output[i].pending.assign(true);
          fetch_source_output[i].data.assign(pending_inst_pos);
        }
      }
    }
  }
};

#endif //RISC_V_REGISTER_FILE_HPP
