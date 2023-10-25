#ifndef NETLIST_PARSER_HPP
#define NETLIST_PARSER_HPP

#include "bytecode.hpp"
#include "lexer.hpp"
#include "report.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>

class Parser {
public:
  explicit Parser(ReportManager &report_manager, Lexer &lexer);

  [[nodiscard]] std::optional<ByteCode> parse_program();

private:
  /// Consumes the current token and gets the next one.
  void consume();

  static constexpr size_t MAX_VARIABLE_SIZE = 64;
  std::optional<size_t> parse_size_specifier();
  bool parse_variables_common(bool allow_size_specifier, const std::function<bool(SourceLocation, std::string_view, size_t)> &handler);
  bool parse_inputs();
  bool parse_outputs();
  bool parse_variables();

  bool parse_equations();
  bool parse_equation();
  bool parse_expression(RegIndex output_reg);
  std::optional<RegIndex> parse_register();
  bool parse_const_expression(RegIndex output_reg);
  bool parse_not_expression(RegIndex output_reg);
  bool parse_reg_expression(RegIndex output_reg);
  bool parse_binary_expression(OpCode binary_opcode, RegIndex output_reg);
  bool parse_mux_expression(RegIndex output_reg);
  bool parse_concat_expression(RegIndex output_reg);
  bool parse_select_expression(RegIndex output_reg);
  bool parse_slice_expression(RegIndex output_reg);
  bool parse_rom_expression(RegIndex output_reg);
  bool parse_ram_expression(RegIndex output_reg);

  void emit_unexpected_token_error(const Token &token, std::string_view expected_token_name);

private:
  ReportManager &m_report_manager;
  Lexer &m_lexer;
  Token m_token;
  ByteCodeWriter m_bytecode_writer;

  struct VariableInfo {
    /// The register allocated to this variable in the generated bytecode.
    RegIndex reg = 0;
    /// The source location where the variable is defined.
    SourceLocation location = INVALID_LOCATION;
    /// Size in bits of the variable as specified in the source code.
    size_t size_in_bits = 1;
    /// Does this variable represents an input? If true, then the variable
    /// was defined inside a `INPUT` declaration.
    bool is_input = false;
    /// Does this variable represents an output? If true, then the variable
    /// was defined inside a `OUTPUT` declaration.
    bool is_output = false;
  };

  std::unordered_map<std::string_view, VariableInfo> m_variables;
};

#endif // NETLIST_PARSER_HPP
