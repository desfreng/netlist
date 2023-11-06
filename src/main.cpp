#include <atomic>
#include <csignal>
#include <fstream>

#include "command_line_parser.hpp"

#include "lexer.hpp"
#include "parser.hpp"

#include "program_printer.hpp"
#include "dot_printer.hpp"
#include "scheduler.hpp"
#include "simulator.hpp"

namespace {
volatile std::atomic_bool stop_flag = false;
}

void signal_handler(int signal) {
  if (signal == SIGTERM || signal == SIGINT) {
    std::cin.setstate(std::ios::failbit); // To detect stopping when reading stdin
    stop_flag = true;
  }
}

std::string read_file(const ReportContext &ctx, std::string_view path) {
  constexpr std::size_t BUFFER_SIZE = 4096;
  static char buffer[BUFFER_SIZE] = {0};

  std::ifstream f = std::ifstream(path.data());
  f.exceptions(std::ios_base::badbit); // To throw error during read

  if (!f.is_open()) {
    ctx.report(ReportSeverity::ERROR)
        .with_message("Error opening file {}", path)
        .with_code(60)
        .build()
        .exit();
  }

  std::string content;
  try {
    while (f.read(buffer, BUFFER_SIZE)) {
      content.append(buffer, 0, f.gcount());
    }
  } catch (std::exception &e) {
    ctx.report(ReportSeverity::ERROR)
        .with_message("Error occurred when reading file {} : {}", path, e.what())
        .with_code(61)
        .build()
        .exit();
  }
  content.append(buffer, 0, f.gcount());

  return content;
}

int main(int argc, const char *argv[]) {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  CommandLineParser cmd_parser(argc, argv);

  if (cmd_parser.get_action() == NoAction) {
    return EXIT_SUCCESS;
  }

  ReportContext ctx(cmd_parser.get_netlist_file(), true);
  std::string netlist_content = read_file(ctx, cmd_parser.get_netlist_file());
  Lexer lexer(ctx, netlist_content.data());
  Parser parser(ctx, lexer);
  Program::ptr program = parser.parse_program();

  switch (cmd_parser.get_action()) {
  case Simulate: {
    InputManager im(cmd_parser.get_inputs());
    Simulator s(ctx, im, program);
    size_t cycle_id = 0;

    if (cmd_parser.cycle_amount_defined()) {
      for (; cycle_id < cmd_parser.get_cycle_amount(); ++cycle_id) {
        if (cmd_parser.is_verbose()) {
          std::cout << "Step " << cycle_id + 1 << ":\n";
        }
        s.cycle();
        if (cmd_parser.is_verbose()) {
          s.print_outputs(std::cout);
          std::cout << "\n";
        }
      }
    } else {
      while (!stop_flag) {
        if (cmd_parser.is_verbose()) {
          std::cout << "Step " << cycle_id + 1 << ":\n";
        }
        s.cycle();
        if (cmd_parser.is_verbose()) {
          s.print_outputs(std::cout);
          std::cout << "\n";
        }
        cycle_id++;
      }
    }
    if (!cmd_parser.is_verbose()) {
      std::cout << "Step " << cycle_id << ":\n";
      s.print_outputs(std::cout);
      std::cout << "\n";
    }
    break;
  }

  case DotExport: {
    DotPrinter printer(program, std::cout);
    printer.print();
    break;
  }

  case PrintFile: {
    ProgramPrinter printer(program, std::cout);
    printer.print();
    break;
  }

  case Schedule: {
    bool is_first = true;

    for (const auto &v : Scheduler::schedule(ctx, program)) {
      if (is_first) {
        is_first = false;
      } else {
        std::cout << " -> ";
      }
      std::cout << v->get_name();
    }
    std::cout << "\n";
    break;
  }
  case NoAction:
    break;
  }

  return EXIT_SUCCESS;
}
