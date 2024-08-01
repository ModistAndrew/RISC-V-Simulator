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
  InstPosWire fetch_inst;
  InstPosWire commit_inst;
  DataWire commit_data;
};

struct RegisterUnitOutput {
  DataWire fetch_data;
  InstPosWire fetch_pending_inst;
  FlagWire fetch_pending;
};

struct RegisterUnitData {
  std::array<RegisterFile, REGISTER_COUNT> register_files;
};

struct RegisterUnit : public dark::Module<RegisterUnitInput, RegisterUnitOutput, RegisterUnitData> {
  void init_wire() {
    fetch_data = [&]() -> auto & { return register_files[to_unsigned(fetch_inst)].data; };
    fetch_pending_inst = [&]() -> auto & { return register_files[to_unsigned(fetch_inst)].pending_inst; };
    fetch_pending = [&]() -> auto & { return register_files[to_unsigned(fetch_inst)].pending; };
  }

  void work() override {
  }
};

#endif //RISC_V_REGISTER_FILE_HPP
