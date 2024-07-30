#include "processor.hpp"
#include "template/cpu.h"

int main() {
  load_instructions();
  dark::CPU cpu;
  ProcessorModule processor;
  cpu.add_module(&processor);
  while (true) {
    cpu.run_once();
  }
}