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
  memory.addr = [&]() -> auto & { return processor.load_addr; };
  memory.mode = [&]() -> auto & { return processor.load_mode; };
  processor.memory_ready = [&]() -> auto & { return memory.ready; };
  processor.memory_busy = [&]() { return memory.phase > 0; };
  processor.memory_data = [&]() -> auto & { return memory.data_out; };
  while (processor.should_return == false) {
    cpu.run_once();
    total_tick++;
  }
  std::cout << to_unsigned(processor.return_value) << std::endl;
  std::cerr << total_committed << "/" << total_tick << std::endl;
  std::cerr << correct_predict << "/" << total_predict << std::endl;
  return 0;
}