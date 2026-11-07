#include "Console.h"

#include <stdarg.h>
#include <stdio.h>

#include "ProjectConfig.h"

void Console::begin() {
  serial_.begin(config::kConsoleBaud);
}

void Console::log(const char* format, ...) {
  // Print::printf aloca memória em mensagens longas, por isso o buffer fixo
  char message[kLogBytes];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  serial_.print("# ");
  serial_.println(message);
}
