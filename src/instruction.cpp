#include "instruction.hpp"

#include <algorithm>
#include <cassert>

#include <fmt/format.h>

/* --------------------------------------------------------
 * class ByteCodeWriter
 */

RegIndex ByteCodeWriter::register_reg(uint8_t bit_width) {
  const RegIndex reg = m_registers.size();
  m_registers.push_back({bit_width});
  return reg;
}

void ByteCodeWriter::write_nop() {
  m_bytecode.push_back(static_cast<std::uint32_t>(OpCode::NOP));
}

void ByteCodeWriter::write_const(const ConstInstruction &data) {
  assert(is_valid_register(data.output));
  write_instruction(OpCode::CONST, data);
}

void ByteCodeWriter::write_not(const NotInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input));
  assert(m_registers[data.output].bit_width == m_registers[data.input].bit_width);
  write_instruction(OpCode::NOT, data);
}

void ByteCodeWriter::write_and(const BinaryInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input_lhs));
  assert(is_valid_register(data.input_rhs));
  assert(m_registers[data.output].bit_width == m_registers[data.input_lhs].bit_width);
  assert(m_registers[data.output].bit_width == m_registers[data.input_rhs].bit_width);
  write_instruction(OpCode::AND, data);
}

void ByteCodeWriter::write_or(const BinaryInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input_lhs));
  assert(is_valid_register(data.input_rhs));
  assert(m_registers[data.output].bit_width == m_registers[data.input_lhs].bit_width);
  assert(m_registers[data.output].bit_width == m_registers[data.input_rhs].bit_width);
  write_instruction(OpCode::OR, data);
}

void ByteCodeWriter::write_nand(const BinaryInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input_lhs));
  assert(is_valid_register(data.input_rhs));
  assert(m_registers[data.output].bit_width == m_registers[data.input_lhs].bit_width);
  assert(m_registers[data.output].bit_width == m_registers[data.input_rhs].bit_width);
  write_instruction(OpCode::NAND, data);
}

void ByteCodeWriter::write_nor(const BinaryInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input_lhs));
  assert(is_valid_register(data.input_rhs));
  assert(m_registers[data.output].bit_width == m_registers[data.input_lhs].bit_width);
  assert(m_registers[data.output].bit_width == m_registers[data.input_rhs].bit_width);
  write_instruction(OpCode::NOR, data);
}

void ByteCodeWriter::write_xor(const BinaryInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input_lhs));
  assert(is_valid_register(data.input_rhs));
  assert(m_registers[data.output].bit_width == m_registers[data.input_lhs].bit_width);
  assert(m_registers[data.output].bit_width == m_registers[data.input_rhs].bit_width);
  write_instruction(OpCode::XOR, data);
}

void ByteCodeWriter::write_reg(const RegInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input));
  assert(m_registers[data.output].bit_width == m_registers[data.input].bit_width);
  write_instruction(OpCode::REG, data);
}

void ByteCodeWriter::write_slice(const SliceInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input));
  assert(data.end > data.first);
  assert(data.first < m_registers[data.input].bit_width);
  assert(data.end < m_registers[data.input].bit_width);
  assert(m_registers[data.output].bit_width == (data.end - data.first + 1));
  write_instruction(OpCode::SLICE, data);
}

void ByteCodeWriter::write_select(const SelectInstruction &data) {
  assert(is_valid_register(data.output));
  assert(is_valid_register(data.input));
  assert(data.i < m_registers[data.input].bit_width);
  assert(m_registers[data.output].bit_width == 1);
  write_instruction(OpCode::SELECT, data);
}

ByteCode ByteCodeWriter::finish() {
  return ByteCode{std::move(m_registers), std::move(m_bytecode)};
}

/* --------------------------------------------------------
 * class Disassembler
 */

void Disassembler::disassemble(const ByteCode &bytecode, std::FILE *stream) {
  fmt::println(stream, "# Registers:");
  for (size_t i = 0; i < bytecode.registers.size(); ++i) {
    const auto &reg_info = bytecode.registers[i];
    fmt::println(stream, "# - r{}: {}", i, reg_info.bit_width);
  }

  Detail printer(bytecode);
  printer.stream = stream;
  printer.read_all();
}

void Disassembler::Detail::handle_nop() const {
  fmt::println(stream, "NOP");
}

void Disassembler::Detail::handle_break() const {
  fmt::println(stream, "BREAK");
}

void Disassembler::Detail::handle_const(const ConstInstruction &inst) const {
  fmt::println(stream, "CONST r{}, {}", inst.output, inst.value);
}

void Disassembler::Detail::handle_not(const NotInstruction &inst) const {
  fmt::println(stream, "NOT r{}, r{}", inst.output, inst.input);
}

void Disassembler::Detail::handle_binary_inst(const char *name, const BinaryInstruction &inst) const {
  fmt::println(stream, "{} r{}, r{}, r{}", name, inst.output, inst.input_lhs, inst.input_rhs);
}

void Disassembler::Detail::handle_and(const BinaryInstruction &inst) const {
  handle_binary_inst("AND", inst);
}

void Disassembler::Detail::handle_or(const BinaryInstruction &inst) const {
  handle_binary_inst("OR", inst);
}

void Disassembler::Detail::handle_nand(const BinaryInstruction &inst) const {
  handle_binary_inst("NAND", inst);
}

void Disassembler::Detail::handle_nor(const BinaryInstruction &inst) const {
  handle_binary_inst("NOR", inst);
}

void Disassembler::Detail::handle_xor(const BinaryInstruction &inst) const {
  handle_binary_inst("XOR", inst);
}

void Disassembler::Detail::handle_reg(const RegInstruction &inst) const {
  fmt::println(stream, "REG r{}, r{}", inst.output, inst.input);
}

void Disassembler::Detail::handle_slice(const SliceInstruction &inst) const {
  fmt::println(stream, "SLICE r{}, r{}, {}, {}", inst.output, inst.input, inst.first, inst.end);
}

void Disassembler::Detail::handle_select(const SelectInstruction &inst) const {
  fmt::println(stream, "SELECT r{}, r{}, {}", inst.output, inst.input, inst.i);
}

/* --------------------------------------------------------
 * class BreakPoint
 */

void BreakPoint::activate(ByteCode &bytecode) {
  saved_word = bytecode.words[offset];
  bytecode.words[offset] = static_cast<std::uint32_t>(OpCode::BREAK);
  is_active = true;
}

void BreakPoint::deactivate(ByteCode &bytecode) {
  bytecode.words[offset] = saved_word;
  is_active = false;
}

/* --------------------------------------------------------
 * class Simulator
 */

// ------------------------
// The registers API

RegValue Simulator::get_register(RegIndex reg) const {
  assert(is_valid_register(reg));

  const auto mask = (1 << m_d.bytecode.registers[reg].bit_width) - 1;
  return m_d.registers_value[reg] & mask;
}

void Simulator::set_register(RegIndex reg, RegValue value) {
  assert(is_valid_register(reg));
  m_d.registers_value[reg] = value;
}

void Simulator::print_registers(std::size_t registers_start, std::size_t registers_end) {
  // Adjust the given register range to be a valid range.
  auto registers_count = registers_end - registers_start + 1;
  registers_count = std::min(m_d.bytecode.registers.size(), registers_count);
  registers_end = registers_start + registers_count;

  fmt::print("Registers:");

  if (registers_start != 0)
    fmt::print("  - ...");

  // Prints the registers value (in binary of course).
  for (std::size_t i = registers_start; i <= registers_end; ++i) {
    const auto &reg_info = m_d.bytecode.registers[i];
    const auto mask = (1 << reg_info.bit_width) - 1;
    const auto current_value = m_d.registers_value[i] & mask;
    const auto previous_value = m_d.previous_registers_value[i] & mask;
    // Prints something like `  - regs[0] = 0b00101 (prev 0b00100)`
    // The `+ 2` in the code below is because we must consider that `0b` is also printed.
    fmt::print("  - r{} = {:#0{}b} (prev {:#0{}b})", i, current_value, reg_info.bit_width + 2, previous_value,
               reg_info.bit_width + 2);
  }

  if (registers_end >= m_d.bytecode.registers.size())
    fmt::print("  - ...");
}

void Simulator::print_ram(std::size_t region_start, std::size_t region_end) {
  // TODO: implement RAM printing
}

void Simulator::execute() {
  while (!(m_d.at_breakpoint || m_d.at_end()))
    step();
}

void Simulator::step() {
  if (m_d.at_breakpoint)
    handle_breakpoint();

  m_d.read_one();
}

std::vector<BreakPoint>::iterator Simulator::find_breakpoint(std::size_t pc) {
  assert(pc < m_d.bytecode.words.size());

  for (auto it = m_d.breakpoints.begin(); it != m_d.breakpoints.end(); ++it) {
    if (it->offset == pc)
      return it;
  }

  return m_d.breakpoints.end();
}

void Simulator::handle_breakpoint() {
  assert(m_d.at_breakpoint);

  auto breakpoint_it = find_breakpoint(m_d.get_position());
  assert(breakpoint_it != m_d.breakpoints.end()); // the breakpoint must exist

  // Temporary deactivate the breakpoint then do a single step of execution.
  // Finally, reactivate the breakpoint.
  m_d.at_breakpoint = false;
  breakpoint_it->deactivate(m_d.bytecode);
  // We are inside a recursive call chain because handle_breakpoint() is called by step().
  // However, we don't have an infinite loop because handle_breakpoint() is only called
  // if at_breakpoint() is true, but we set it to false just above.
  step();

  // Handle oneshot breakpoints, that is breakpoints that only works one time:
  if (breakpoint_it->oneshot) {
    m_d.breakpoints.erase(breakpoint_it);
  } else {
    breakpoint_it->activate(m_d.bytecode);
  }
}

Simulator::Detail::Detail(const ByteCode &bytecode) : ByteCodeReader<Detail>(bytecode), bytecode(bytecode) {
  registers_value = std::make_unique<RegValue[]>(bytecode.registers.size());
  previous_registers_value = std::make_unique<RegValue[]>(bytecode.registers.size());
}

#define SIMULATOR_PEDANTIC_CHECKS

// If we really want to detect all possible errors at runtime, we can enable SIMULATOR_PEDANTIC_CHECKS
// which adds many asserts into the simulator code. This may be quite useful during development or
// to debug the simulator, however its slows down the simulation.
#ifdef SIMULATOR_PEDANTIC_CHECKS
#define SIMULATOR_ASSERT(expr) assert(expr)
#else
#define SIMULATOR_ASSERT(expr)
#endif

// Check if the given register index is in the bounds declared by the bytecode.
// As you may suspect, it is not good to have a bytecode that contains instruction accessing
// registers that do not exist. However, in practice this should never happen as the
// bytecode is generated by ByteCodeWriter which do not generate ill-formed register access.
#define SIMULATOR_CHECK_REG(reg) SIMULATOR_ASSERT((reg) < bytecode.registers.size())
// Checks if both the given register index are referencing registers of the same bit width.
// Normally, these checks are done at compile time by the parser. But just to be pedantic,
// you can use this macro to check at the simulation time.
#define SIMULATOR_CHECK_BIT_WIDTH(l, r)                                                                                \
  SIMULATOR_ASSERT(bytecode.registers[(l)].bit_width == bytecode.registers[(r)].bit_width)

void Simulator::Detail::handle_break() {
  at_breakpoint = true;
}

void Simulator::Detail::handle_const(const ConstInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.output);

  registers_value[inst.output] = inst.value;
}

void Simulator::Detail::handle_not(const NotInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input, inst.output);

  // We don't want a logical not but a bitwise not.
  registers_value[inst.output] = ~(registers_value[inst.input]);
}

void Simulator::Detail::handle_and(const BinaryInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input_lhs);
  SIMULATOR_CHECK_REG(inst.input_rhs);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_lhs, inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_rhs, inst.output);

  const auto lhs = registers_value[inst.input_lhs];
  const auto rhs = registers_value[inst.input_rhs];
  // We don't want a logical and but a bitwise and.
  registers_value[inst.output] = lhs & rhs;
}

void Simulator::Detail::handle_or(const BinaryInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input_lhs);
  SIMULATOR_CHECK_REG(inst.input_rhs);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_lhs, inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_rhs, inst.output);

  const auto lhs = registers_value[inst.input_lhs];
  const auto rhs = registers_value[inst.input_rhs];
  // We don't want a logical or but a bitwise or.
  registers_value[inst.output] = lhs | rhs;
}

void Simulator::Detail::handle_nand(const BinaryInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input_lhs);
  SIMULATOR_CHECK_REG(inst.input_rhs);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_lhs, inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_rhs, inst.output);

  const auto lhs = registers_value[inst.input_lhs];
  const auto rhs = registers_value[inst.input_rhs];
  // We let the C++ compiler select instruction to implement NAND (there is no NAND operator in C++).
  registers_value[inst.output] = ~(lhs & rhs);
}

void Simulator::Detail::handle_nor(const BinaryInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input_lhs);
  SIMULATOR_CHECK_REG(inst.input_rhs);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_lhs, inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_rhs, inst.output);

  const auto lhs = registers_value[inst.input_lhs];
  const auto rhs = registers_value[inst.input_rhs];
  // We let the C++ compiler select instruction to implement NOR (there is no NOR operator in C++).
  registers_value[inst.output] = ~(lhs | rhs);
}

void Simulator::Detail::handle_xor(const BinaryInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input_lhs);
  SIMULATOR_CHECK_REG(inst.input_rhs);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_lhs, inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input_rhs, inst.output);

  const auto lhs = registers_value[inst.input_lhs];
  const auto rhs = registers_value[inst.input_rhs];
  registers_value[inst.output] = lhs ^ rhs;
}

void Simulator::Detail::handle_reg(const RegInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input);
  SIMULATOR_CHECK_REG(inst.output);
  SIMULATOR_CHECK_BIT_WIDTH(inst.input, inst.output);

  const auto previous_value = previous_registers_value[inst.input];
  registers_value[inst.output] = previous_value;
}

void Simulator::Detail::handle_slice(const SliceInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input);

  // The `+ 1` is because both end and first are inclusives.
  const auto bit_width = inst.end - inst.first + 1;
  SIMULATOR_ASSERT(bytecode.registers[inst.output].bit_width == bit_width);

  const auto value = registers_value[inst.input];
  // Mask is a binary integer whose least significant bit_width bits are set to 1.
  const auto mask = (1 << (bit_width)) - 1;
  registers_value[inst.output] = (value >> inst.first) & mask;
}

void Simulator::Detail::handle_select(const SelectInstruction &inst) {
  SIMULATOR_CHECK_REG(inst.input);
  SIMULATOR_ASSERT(bytecode.registers[inst.output].bit_width == 1);

  const auto value = registers_value[inst.input];
  registers_value[inst.output] = (value >> inst.i) & 0b1;
}
