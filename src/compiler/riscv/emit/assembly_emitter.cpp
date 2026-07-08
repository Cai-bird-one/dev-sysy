#include "compiler/riscv/emit/assembly_emitter.h"

#include <ostream>

namespace compiler::riscv {

AssemblyEmitter::AssemblyEmitter(std::ostream &output) : output_(output) {}

void AssemblyEmitter::sectionText() { output_ << "  .text\n"; }

void AssemblyEmitter::sectionData() { output_ << "  .data\n"; }

void AssemblyEmitter::sectionBss() { output_ << "  .bss\n"; }

void AssemblyEmitter::global(const std::string &name) {
  output_ << "  .globl " << name << "\n";
}

void AssemblyEmitter::label(const std::string &name) {
  output_ << name << ":\n";
}

void AssemblyEmitter::word(int value) {
  output_ << "  .word " << value << "\n";
}

void AssemblyEmitter::zero(int bytes) { output_ << "  .zero " << bytes << "\n"; }

void AssemblyEmitter::instruction(const std::string &text) {
  output_ << "  " << text << "\n";
}

void AssemblyEmitter::loadImmediate(const std::string &reg, int value) {
  output_ << "  li " << reg << ", " << value << "\n";
}

void AssemblyEmitter::loadImmediate(const std::string &reg,
                                    const std::string &value) {
  output_ << "  li " << reg << ", " << value << "\n";
}

void AssemblyEmitter::loadAddress(const std::string &reg,
                                  const std::string &symbol) {
  output_ << "  la " << reg << ", " << symbol << "\n";
}

void AssemblyEmitter::loadWord(const std::string &reg, int offset,
                               const std::string &base) {
  if (fitsSigned12Bit(offset)) {
    output_ << "  lw " << reg << ", " << offset << "(" << base << ")\n";
    return;
  }
  loadImmediate("t2", offset);
  output_ << "  add t2, " << base << ", t2\n"
          << "  lw " << reg << ", 0(t2)\n";
}

void AssemblyEmitter::storeWord(const std::string &reg, int offset,
                                const std::string &base) {
  if (fitsSigned12Bit(offset)) {
    output_ << "  sw " << reg << ", " << offset << "(" << base << ")\n";
    return;
  }
  loadImmediate("t2", offset);
  output_ << "  add t2, " << base << ", t2\n"
          << "  sw " << reg << ", 0(t2)\n";
}

void AssemblyEmitter::loadStackAddress(int offset, const std::string &reg) {
  if (fitsSigned12Bit(offset)) {
    output_ << "  addi " << reg << ", sp, " << offset << "\n";
    return;
  }
  loadImmediate(reg, offset);
  output_ << "  add " << reg << ", sp, " << reg << "\n";
}

void AssemblyEmitter::adjustStack(int amount) {
  if (amount == 0) {
    return;
  }
  if (fitsSigned12Bit(amount)) {
    output_ << "  addi sp, sp, " << amount << "\n";
    return;
  }
  loadImmediate("t0", amount);
  output_ << "  add sp, sp, t0\n";
}

bool AssemblyEmitter::fitsSigned12Bit(int value) const {
  return value >= -2048 && value <= 2047;
}

} // namespace compiler::riscv
