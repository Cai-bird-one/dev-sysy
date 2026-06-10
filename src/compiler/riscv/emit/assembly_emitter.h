#pragma once

#include <iosfwd>
#include <string>

namespace compiler::riscv {

class AssemblyEmitter {
public:
  explicit AssemblyEmitter(std::ostream &output);

  void sectionText();
  void sectionData();
  void global(const std::string &name);
  void label(const std::string &name);
  void word(int value);

  void instruction(const std::string &text);
  void loadImmediate(const std::string &reg, int value);
  void loadImmediate(const std::string &reg, const std::string &value);
  void loadAddress(const std::string &reg, const std::string &symbol);
  void loadWord(const std::string &reg, int offset,
                const std::string &base = "sp");
  void storeWord(const std::string &reg, int offset,
                 const std::string &base = "sp");
  void loadStackAddress(int offset, const std::string &reg);
  void adjustStack(int amount);

private:
  bool fitsSigned12Bit(int value) const;

  std::ostream &output_;
};

} // namespace compiler::riscv
