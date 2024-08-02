#include "processor.hpp"
#include "memory.hpp"
#include "constants.hpp"
#include "instructions.hpp"
#include "program_counter.hpp"
#include "register_unit.hpp"
#include "template/cpu.h"

int main() {
  freopen("../testcases/magic.data", "r", stdin);
  memory::load_instructions();
  dark::CPU cpu;
  ProcessorModule processor;
  cpu.add_module(&processor);
  while (processor.should_return == false) {
    cpu.run_once();
  }
  std::cout << to_unsigned(processor.return_value) << std::endl;
  return 0;
}