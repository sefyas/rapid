#ifndef _RAPID_UTILS_COMMAND_LINE_H_
#define _RAPID_UTILS_COMMAND_LINE_H_

#include <string>
#include <vector>

#include "command_interface.h"

namespace rapid {
namespace utils {
class ExitCommand : public CommandInterface {
 public:
  void Execute(const std::vector<std::string>& args);
  std::string name() const;
  std::string description() const;
};

class CommandLine {
 public:
  CommandLine();
  CommandLine(const std::string& name);

  // Add a command to this command line.
  void AddCommand(CommandInterface* command);

  // Runs one loop of the command line, and returns whether there should be
  // another loop. Each loop shows the command list, parses the given command,
  // and executes the command. If the command is invalid, it does nothing and
  // returns true. If the user types "exit" or Ctrl+D, this returns false.
  bool Next();

 private:
  void ShowCommands() const;
  bool ParseLine(const std::string& line, CommandInterface** command_pp,
                 std::vector<std::string>* args) const;
  int ParseCommand(const std::vector<std::string>& tokens,
                   const std::string& name) const;
  std::string name_;
  std::vector<CommandInterface*> commands_;
};
}  // namespace utils
}  // namespace rapid

#endif  // _RAPID_UTILS_COMMAND_LINE_H_
