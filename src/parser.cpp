#include "parser.hpp"
#include <sstream>

/// Parse the current token as an integer in the given base. We use a template to process different integer types.
template<class T>
[[nodiscard]] T parse_int(const Token &token, int base, ReportContext &c) {
  T v;

  auto [ptr, ec] = std::from_chars(token.spelling.begin(), token.spelling.end(), v, base);
  if (ec == std::errc::result_out_of_range) {
    c.report(ReportSeverity::ERROR)
        .with_location(token.position)
        .with_message("The value '{}' (interpreted in base {}) is too big to fit in a {}. "
                      "Max value authorised : '{}'",
                      token.spelling, base, typeid(T).name(), std::numeric_limits<T>::max())
        .build()
        .exit();
  } else if (ec == std::errc::invalid_argument) {
    c.report(ReportSeverity::ERROR)
        .with_location(token.position)
        .with_message("Error parsing value '{}' in base {}", token.spelling, base)
        .build()
        .exit();
  } else if (*ptr != '\0') {
    c.report(ReportSeverity::ERROR)
        .with_location(token.position)
        .with_message("Error parsing value '{}' in base {}. Successfully parsed the value '{}' "
                      "but unable to parse this part : '{}'", token.spelling, base, v, ptr)
        .build()
        .exit();
  }

  return v;
}

const std::unordered_map<TokenKind, std::string> token_spelling = {
    {TokenKind::EOI, "End-Of-File"},
    {TokenKind::IDENTIFIER, "Identifier (such as '_l_10')"},
    {TokenKind::INTEGER, "Integer (such as '42' or '0100101010110' or '1')"},
    {TokenKind::BINARY_CONSTANT, "Binary Constant (such as '0b011011' or '0b1')"},
    {TokenKind::DECIMAL_CONSTANT, "Decimal Constant (such as '0d215')"},
    {TokenKind::HEXADECIMAL_CONSTANT, "Hexadecimal Constant (such as '0xf2f')"},

    {TokenKind::EQUAL, "="},
    {TokenKind::COMMA, ","},
    {TokenKind::COLON, ":"},

    {TokenKind::KEY_INPUT, "INPUT"},
    {TokenKind::KEY_OUTPUT, "OUTPUT"},
    {TokenKind::KEY_VAR, "VAR"},
    {TokenKind::KEY_IN, "IN"},

    {TokenKind::KEY_NOT, "NOT"},
    {TokenKind::KEY_AND, "AND"},
    {TokenKind::KEY_NAND, "NAND"},
    {TokenKind::KEY_OR, "OR"},
    {TokenKind::KEY_XOR, "XOR"},

    {TokenKind::KEY_MUX, "MUX"},
    {TokenKind::KEY_REG, "REG"},
    {TokenKind::KEY_CONCAT, "CONCAT"},
    {TokenKind::KEY_SELECT, "SELECT"},
    {TokenKind::KEY_SLICE, "SLICE"},
    {TokenKind::KEY_ROM, "ROM"},
    {TokenKind::KEY_RAM, "RAM"},
};

Parser::Parser(ReportContext &context, Lexer &lexer)
    : m_context(context), m_lexer(lexer) {
  // Gets the first token
  consume();
}

Program::ptr Parser::parse_program() {
  // Remove any existing variable
  var_decl.clear();
  vars.clear();

  // Target Program
  Program::ptr p(new Program());

  // m_token.kind = TokenKind::KEY_INPUT
  const var_ref_map in_refs = parse_inputs_references();
  // m_token.kind = TokenKind::KEY_OUTPUT
  const var_ref_map out_refs = parse_outputs_references();
  // m_token.kind = TokenKind::KEY_VAR
  parse_variable_declaration();
  // m_token.kind = TokenKind::KEY_IN
  build_intput_output_list(p, in_refs, out_refs);

  // Now we parse each equation
  parse_equations(p);
  // m_token.kind = TokenKind::EOI

  for (auto &decl_pair : vars) {
    const Variable::ptr &declared_var = decl_pair.second;

    if (in_refs.contains(declared_var->get_name())) {
      p->m_vars.push_back(declared_var);
      continue; // 'declared_var' is a input.
    }

    if (p->m_eq.contains(declared_var)) {
      p->m_vars.push_back(declared_var);
      continue; // 'declared_var' has an equation
    }

    // No equation and no input
    m_context.report(ReportSeverity::ERROR)
        .with_location(var_decl.at(declared_var->get_name()).position)
        .with_message("Declared variable '{}' does not have an associated equation.", declared_var->get_name())
        .build()
        .exit();

  }

  // Remove any existing variable
  var_decl.clear();
  vars.clear();

  return p;
}

[[noreturn]] void Parser::unexpected_token(const std::set<TokenKind> &expected) const {
  std::stringstream tokens;

  bool first = true;

  for (const TokenKind &tok : expected) {
    if (first) {
      first = false;
    } else {
      tokens << "' or '";
    }

    tokens << token_spelling.at(tok);
  }

  m_context.report(ReportSeverity::ERROR)
      .with_location(m_token.position)
      .with_message("Unexpected token. Found : '{}', expected : '{}'", m_token.spelling, tokens.str())
      .build()
      .exit();
}

void Parser::token_assert(const std::set<TokenKind> &token_set) const {
  if (!token_set.contains(m_token.kind)) {
    unexpected_token(token_set);
  }
}

void Parser::token_assert(TokenKind token) const {
  if (m_token.kind != token) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("Unexpected token. Found : '{}', expected : '{}'", m_token.spelling, token_spelling.at(token))
        .build()
        .exit();
  }
}

void Parser::assert_same_bus_size(const Argument::ptr &arg1, const Argument::ptr &arg2, const SourcePosition &pos) {
  if (arg1->get_bus_size() != arg2->get_bus_size()) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(pos)
        .with_message("The two arguments '{}' (bus size : {}) and '{}' (bus size : {}) should have the same bus size.",
                      arg1->get_repr(), arg1->get_bus_size(), arg2->get_repr(), arg2->get_bus_size())
        .build()
        .exit();
  }
}

void Parser::assert_bus_size_eq(const Argument::ptr &arg, bus_size_t size, const SourcePosition &pos) {
  if (arg->get_bus_size() != size) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(pos)
        .with_message("The argument '{}' (bus size : {}) should have a bus size of {}.",
                      arg->get_repr(), arg->get_bus_size(), size)
        .build()
        .exit();
  }
}

void Parser::assert_bus_size_gt(const Argument::ptr &arg, bus_size_t size, const SourcePosition &pos) {
  if (arg->get_bus_size() <= size) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(pos)
        .with_message("The argument '{}' (bus size : {}) should have a bus size strictly greater than {}.",
                      arg->get_repr(), arg->get_bus_size(), size)
        .build()
        .exit();
  }
}

Parser::var_ref_map Parser::parse_inputs_references() {
  token_assert(TokenKind::KEY_INPUT);
  consume(); // eat 'INPUT'

  var_ref_map var;

  while (m_token.kind != TokenKind::KEY_OUTPUT) {
    token_assert(TokenKind::IDENTIFIER);

    if (var.contains(m_token.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(m_token.position)
          .with_message(
              "The variable '{}' has already been marked as input at {}.",
              m_token.spelling,
              m_context.get_location(var[m_token.spelling].position))
          .build()
          .exit();
    }

    var[m_token.spelling] = {m_token.spelling, m_token.position};
    consume(); // eat 'IDENTIFIER'

    token_assert({TokenKind::COMMA, TokenKind::KEY_OUTPUT});

    if (m_token.kind == TokenKind::COMMA) {
      consume(); // eat 'COMMA'
    }
  }

  return var;
}

Parser::var_ref_map Parser::parse_outputs_references() {
  token_assert(TokenKind::KEY_OUTPUT);
  consume(); // eat 'OUTPUT'

  var_ref_map var;

  while (m_token.kind != TokenKind::KEY_VAR) {
    token_assert(TokenKind::IDENTIFIER);
    if (var.contains(m_token.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(m_token.position)
          .with_message(
              "The variable '{}' has already been marked as output at {}.",
              m_token.spelling,
              m_context.get_location(var[m_token.spelling].position))
          .build()
          .exit();
    }

    var[m_token.spelling] = {m_token.spelling, m_token.position};
    consume(); // eat 'IDENTIFIER'

    token_assert({TokenKind::COMMA, TokenKind::KEY_VAR});

    if (m_token.kind == TokenKind::COMMA) {
      consume(); // eat 'COMMA'
    }
  }

  return var;
}

void Parser::parse_variable_declaration() {
  token_assert(TokenKind::KEY_VAR);
  consume(); // eat 'VAR'

  while (m_token.kind != TokenKind::KEY_IN) {
    token_assert(TokenKind::IDENTIFIER);

    VariableDeclaration v = {};
    v.spelling = m_token.spelling;
    v.position = m_token.position;
    consume(); // eat 'IDENTIFIER'

    if (m_token.kind == TokenKind::COLON) {
      consume(); // eat ':'
      v.size = parse_bus_size();
    } else {
      v.size = 1;
    }

    if (var_decl.contains(v.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(v.position)
          .with_message("The variable '{}' has already been declared at {}.",
                        v.spelling,
                        m_context.get_location(var_decl[v.spelling].position))
          .build()
          .exit();
    }

    vars.try_emplace(v.spelling, std::make_shared<Variable>(v.size, v.spelling));
    var_decl[v.spelling] = v;

    token_assert({TokenKind::COMMA, TokenKind::KEY_IN});

    if (m_token.kind == TokenKind::COMMA) {
      consume(); // eat 'COMMA'
      token_assert(TokenKind::IDENTIFIER);
    }
  }
}

void Parser::build_intput_output_list(Program::ptr &p, const var_ref_map &in_refs, const var_ref_map &out_refs) {
  for (auto &in_pair : in_refs) {
    // We only iterate over the VariableReference value of the hashtable.
    const VariableReference &i_ref = in_pair.second;
    if (!vars.contains(i_ref.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(i_ref.position)
          .with_message(
              "Missing declaration of input '{}' in the variable section.",
              i_ref.spelling)
          .build()
          .exit();
    }

    p->m_input.emplace_back(vars.at(i_ref.spelling));
  }

  for (auto &out_pair : out_refs) {
    // We only iterate over the VariableReference value of the hashtable.
    const VariableReference &o_ref = out_pair.second;
    if (!vars.contains(o_ref.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(o_ref.position)
          .with_message(
              "Missing declaration of output '{}' in the variable section.",
              o_ref.spelling)
          .build()
          .exit();
    }

    p->m_output.emplace_back(vars.at(o_ref.spelling));
  }
}

void Parser::parse_equations(Program::ptr &p) {
  token_assert(TokenKind::KEY_IN);
  consume(); // eat 'IN'

  while (m_token.kind != TokenKind::EOI) {
    token_assert(TokenKind::IDENTIFIER);
    if (!vars.contains(m_token.spelling)) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(m_token.position)
          .with_message("Assigment of undefined variable '{}'",
                        m_token.spelling)
          .build()
          .exit();
    }
    const Variable::ptr &assigment_var = vars.at(m_token.spelling);
    consume(); // eat the variable
    token_assert(TokenKind::EQUAL);
    consume(); // eat '='
    Expression::ptr eq = parse_expression();
    if (eq->get_bus_size() != assigment_var->get_bus_size()) {
      // Wrong bus size.
    }
    p->m_eq.try_emplace(assigment_var, std::move(eq));
  }
}

Expression::ptr Parser::parse_expression() {
  const SourcePosition expr_pos = m_token.position;

  switch (m_token.kind) {
  case TokenKind::IDENTIFIER: {
    return std::make_unique<ArgExpression>(parse_variable());
  }

  case TokenKind::INTEGER: {
    return std::make_unique<ArgExpression>(parse_binary_digits());
  }

  case TokenKind::BINARY_CONSTANT: {
    return std::make_unique<ArgExpression>(parse_binary_constant());
  }

  case TokenKind::DECIMAL_CONSTANT: {
    return std::make_unique<ArgExpression>(parse_decimal_constant());
  }

  case TokenKind::HEXADECIMAL_CONSTANT: {
    return std::make_unique<ArgExpression>(parse_hexadecimal_constant());
  }

  case TokenKind::KEY_NOT: {
    consume();

    return std::make_unique<NotExpression>(parse_argument());
  }

  case TokenKind::KEY_AND: {
    consume();

    const Argument::ptr lhs = parse_argument();
    const Argument::ptr rhs = parse_argument();

    assert_same_bus_size(lhs, rhs, expr_pos);

    return std::make_unique<BinOpExpression>(BinOpExpression::BinOpKind::AND, lhs, rhs);
  }

  case TokenKind::KEY_NAND: {
    consume();

    const Argument::ptr lhs = parse_argument();
    const Argument::ptr rhs = parse_argument();

    assert_same_bus_size(lhs, rhs, expr_pos);

    return std::make_unique<BinOpExpression>(BinOpExpression::BinOpKind::NAND, lhs, rhs);
  }

  case TokenKind::KEY_OR: {
    consume();

    const Argument::ptr lhs = parse_argument();
    const Argument::ptr rhs = parse_argument();

    assert_same_bus_size(lhs, rhs, expr_pos);

    return std::make_unique<BinOpExpression>(BinOpExpression::BinOpKind::OR, lhs, rhs);
  }

  case TokenKind::KEY_XOR: {
    consume();

    const Argument::ptr lhs = parse_argument();
    const Argument::ptr rhs = parse_argument();

    assert_same_bus_size(lhs, rhs, expr_pos);

    return std::make_unique<BinOpExpression>(BinOpExpression::BinOpKind::XOR, lhs, rhs);
  }

  case TokenKind::KEY_MUX: {
    consume();

    const Argument::ptr choice = parse_argument();
    const Argument::ptr true_branch = parse_argument();
    const Argument::ptr false_branch = parse_argument();

    assert_bus_size_eq(choice, 1, expr_pos);
    assert_same_bus_size(true_branch, false_branch, expr_pos);

    return std::make_unique<MuxExpression>(choice, true_branch, false_branch);
  }

  case TokenKind::KEY_REG: {
    consume();

    return std::make_unique<RegExpression>(parse_variable());
  }

  case TokenKind::KEY_CONCAT: {
    consume();

    const Argument::ptr arg_beg = parse_argument();
    const Argument::ptr arg_end = parse_argument();

    return std::make_unique<ConcatExpression>(arg_beg, arg_end);
  }

  case TokenKind::KEY_SELECT: {
    consume();

    const bus_size_t index = parse_bus_size();
    const Argument::ptr arg = parse_argument();

    assert_bus_size_gt(arg, index, expr_pos);

    return std::make_unique<SelectExpression>(index, arg);
  }

  case TokenKind::KEY_SLICE: {
    consume();

    const bus_size_t beg = parse_bus_size();
    const bus_size_t end = parse_bus_size();
    const Argument::ptr arg = parse_argument();

    if (beg >= end) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(expr_pos)
          .with_message("The beginning of the interval ({}) must be less than the end of the interval ({}).", beg, end)
          .build()
          .exit();
    }

    assert_bus_size_gt(arg, end, expr_pos);

    return std::make_unique<SliceExpression>(beg, end, arg);
  }

  case TokenKind::KEY_ROM: {
    consume();

    const bus_size_t addr_size = parse_bus_size();
    const bus_size_t word_size = parse_bus_size();
    const Argument::ptr read_addr = parse_argument();

    assert_bus_size_eq(read_addr, addr_size, expr_pos);

    return std::make_unique<RomExpression>(addr_size, word_size, read_addr);
  }

  case TokenKind::KEY_RAM: {
    consume();

    const bus_size_t addr_size = parse_bus_size();
    const bus_size_t word_size = parse_bus_size();
    const Argument::ptr read_addr = parse_argument();
    const Argument::ptr write_enable = parse_argument();
    const Argument::ptr write_addr = parse_argument();
    const Argument::ptr data = parse_argument();

    assert_bus_size_eq(read_addr, addr_size, expr_pos);
    assert_bus_size_eq(write_enable, 1, expr_pos);
    assert_bus_size_eq(write_addr, addr_size, expr_pos);
    assert_bus_size_eq(data, word_size, expr_pos);

    return std::make_unique<RamExpression>(addr_size, word_size, read_addr, write_enable, write_addr, data);
  }

  case TokenKind::EOI:
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("Missing expression for assigment.")
        .build()
        .exit();

  case TokenKind::EQUAL:
  case TokenKind::COMMA:
  case TokenKind::COLON:
  case TokenKind::KEY_OUTPUT:
  case TokenKind::KEY_INPUT:
  case TokenKind::KEY_VAR:
  case TokenKind::KEY_IN:
    unexpected_token({TokenKind::IDENTIFIER, TokenKind::INTEGER, TokenKind::BINARY_CONSTANT,
                      TokenKind::DECIMAL_CONSTANT, TokenKind::HEXADECIMAL_CONSTANT, TokenKind::KEY_NOT,
                      TokenKind::KEY_AND, TokenKind::KEY_NAND, TokenKind::KEY_OR, TokenKind::KEY_XOR,
                      TokenKind::KEY_MUX, TokenKind::KEY_REG, TokenKind::KEY_CONCAT, TokenKind::KEY_SELECT,
                      TokenKind::KEY_SLICE, TokenKind::KEY_ROM, TokenKind::KEY_RAM});
  }
}

bus_size_t Parser::parse_bus_size() {
  token_assert(TokenKind::INTEGER);

  auto bs = parse_int<bus_size_t>(m_token, 10, m_context); // We parse in base 10 here.

  if (bs > max_bus_size) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("Integer '{}' is too big to be a bus size. Max bus size authorised : '{}'", bs, max_bus_size)
        .build()
        .exit();
  }

  consume();
  return bs;
}

std::optional<bus_size_t> Parser::parse_size_spec() {
  if (m_token.kind == TokenKind::COLON) {
    consume(); // eat ':'
    return parse_bus_size();
  }

  return {};
}

Constant::ptr Parser::parse_binary_digits() {
  token_assert(TokenKind::INTEGER);

  auto val = parse_int<value_t>(m_token, 2, m_context); // We parse in base 2 here.

  consume();
  return std::make_shared<Constant>(m_token.spelling.size(), val);
}

Constant::ptr Parser::parse_binary_constant() {
  token_assert(TokenKind::BINARY_CONSTANT);

  const Token cst_tok = m_token; // We copy the token in case of no size specifier
  auto val = parse_int<value_t>(m_token, 2, m_context); // We parse in base 2 here
  consume();

  auto size = parse_size_spec();

  if (size.has_value()) {
    if (val > Bus::max_value(size.value())) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(m_token.position)
          .with_message("The binary value '{}' (decimal : {}) is too large to fit in a bus size of {}. "
                        "The maximum authorised value for the variable is : {}",
                        cst_tok.spelling, val, size.value(), Bus::max_value(size.value()))
          .build()
          .exit();
    }
    return std::make_shared<Constant>(size.value(), val);
  } else {
    return std::make_shared<Constant>(cst_tok.spelling.size(), val); // '0b' prefix is no in the token spelling
  }
}

Constant::ptr Parser::parse_decimal_constant() {
  token_assert(TokenKind::DECIMAL_CONSTANT);

  auto val = parse_int<value_t>(m_token, 10, m_context); // We parse in base 10 here
  consume();
  auto size = parse_size_spec();

  if (!size.has_value()) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("The decimal constant '{}' should have a size specifier.", val)
        .build()
        .exit();
  }

  if (val > Bus::max_value(size.value())) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("The decimal value '{}' is too large to fit in a bus size of {}. "
                      "The maximum authorised value for the variable is : {}",
                      val, size.value(), Bus::max_value(size.value()))
        .build()
        .exit();
  }

  return std::make_shared<Constant>(size.value(), val);
}

Constant::ptr Parser::parse_hexadecimal_constant() {
  token_assert(TokenKind::HEXADECIMAL_CONSTANT);

  const Token cst_tok = m_token; // We copy the token in case of no size specifier
  auto val = parse_int<value_t>(m_token, 16, m_context); // We parse in base 16 here
  consume();

  auto size = parse_size_spec();

  if (size.has_value()) {
    if (val > Bus::max_value(size.value())) {
      m_context.report(ReportSeverity::ERROR)
          .with_location(m_token.position)
          .with_message("The hexadecimal value '{}' (decimal : {}) is too large to fit in a bus size of {}. "
                        "The maximum authorised value for the variable is : {}",
                        cst_tok.spelling, val, size.value(), Bus::max_value(size.value()))
          .build()
          .exit();
    }
    return std::make_shared<Constant>(size.value(), val);
  } else {
    // One hexadecimal digit <-> 4 bits
    return std::make_shared<Constant>(cst_tok.spelling.size() * 4, val); // '0x' prefix is no in the token spelling
  }
}

Variable::ptr Parser::parse_variable() {
  token_assert(TokenKind::IDENTIFIER);

  if (!vars.contains(m_token.spelling)) {
    m_context.report(ReportSeverity::ERROR)
        .with_location(m_token.position)
        .with_message("Undefined variable {}.", m_token.spelling)
        .build()
        .exit();
  }

  const Variable::ptr &v = vars.at(m_token.spelling);
  consume();
  return v;
}

Argument::ptr Parser::parse_argument() {
  if (m_token.kind == TokenKind::IDENTIFIER) {
    return parse_variable();
  } else if (m_token.kind == TokenKind::INTEGER) {
    return parse_binary_digits();
  } else if (m_token.kind == TokenKind::BINARY_CONSTANT) {
    return parse_binary_constant();
  } else if (m_token.kind == TokenKind::DECIMAL_CONSTANT) {
    return parse_decimal_constant();
  } else if (m_token.kind == TokenKind::HEXADECIMAL_CONSTANT) {
    return parse_hexadecimal_constant();
  } else {
    unexpected_token({TokenKind::IDENTIFIER,
                      TokenKind::INTEGER,
                      TokenKind::BINARY_CONSTANT,
                      TokenKind::DECIMAL_CONSTANT,
                      TokenKind::HEXADECIMAL_CONSTANT});
  }
}
