#include "processor.hpp"
#include "memory.hpp"
#include "template/cpu.h"

int main() {
  freopen("../testcases/magic.data", "r", stdin);
  memory::load_instructions();
  dark::CPU cpu;
  ProcessorModule processor;
  Memory memory;
  cpu.add_module(&processor);
  cpu.add_module(&memory);
  memory.load = [&]() -> auto & { return processor.load; };
  memory.store = [&]() -> auto & { return processor.store; };
  memory.addr = [&]() -> auto & { return processor.addr; };
  memory.mode = [&]() -> auto & { return processor.memory_mode; };
  memory.store_data = [&]() -> auto & { return processor.store_data; };
  memory.flushing = [&]() -> auto & { return processor.flushing; };
  processor.memory_load_finished = [&]() { return memory.phase == 1; };
  processor.memory_store_finished = [&]() { return memory.phase == -1; };
  processor.memory_busy = [&]() { return memory.phase != 0; };
  processor.memory_data = [&]() -> auto & { return memory.data_out; };
  while (processor.should_return == false) {
    cpu.run_once_shuffle();
    total_tick++;
  }
  std::cout << to_unsigned(processor.return_value) << std::endl;
  std::cerr << total_committed << "/" << total_tick << std::endl;
  std::cerr << correct_predict << "/" << total_predict << std::endl;
  return 0;
}