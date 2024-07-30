#include "processor.hpp"
#include "template/cpu.h"

int main() {
  init();
  load_instructions();
  dark::CPU cpu;
  ProcessorModule processor;
  cpu.add_module(&processor);
  while (processor.should_return == false) {
    cpu.run_once();
  }
  std::cout << to_signed(processor.return_value) << std::endl;
  return 0;
}