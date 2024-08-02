//
// Created by zjx on 2024/7/31.
//

#ifndef RISC_V_CONSTANT_HPP
#define RISC_V_CONSTANT_HPP

#include "template/tools.h"

constexpr unsigned int REGISTER_COUNT = 1 << 5;
constexpr unsigned int INSTRUCTION_BUFFER_SIZE = 1 << 4;
constexpr unsigned int PREDICTOR_HASH_SIZE = 1 << 4;
using InstPos = Register<4>; // a position in instruction buffer.
using InstPosWire = Wire<4>;
using OpCode = Register<7>;
using Data = Register<32>; // data or memory address
using DataWire = Wire<32>;
using RegPos = Register<5>;
using Flag = Register<1>;
using FlagWire = Wire<1>;
using Return = Register<8>;
using PredictorStatusCode = Register<2>;
using MemoryAccessModeCode = Register<3>;
using MemoryAccessModeWire = Wire<3>;
using Byte = Bit<8>;
using HalfWord = Bit<16>;
using Word = Bit<32>;
int total_predict;
int correct_predict;
int total_tick;
int total_committed;

#endif //RISC_V_CONSTANT_HPP
