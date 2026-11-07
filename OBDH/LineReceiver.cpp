#include "LineReceiver.h"

#include <string.h>

namespace {

bool isPrintableAscii(const char* text, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < 0x20 || text[i] > 0x7E) {
      return false;
    }
  }
  return true;
}

// Linha de dados: id inteiro seguido dos campos numéricos, que podem estar vazios.
// A linha serial não tem checksum, então um erro que preserve este formato passa.
bool isDataLine(const char* text, size_t length) {
  uint8_t commas = 0;
  for (size_t i = 0; i < length; ++i) {
    const char c = text[i];
    if (c == ',') {
      ++commas;
    } else if (commas == 0 ? (c < '0' || c > '9')
                           : ((c < '0' || c > '9') && c != '.' && c != '-')) {
      return false;
    }
  }
  return commas == config::kCsvColumns - 1 && text[0] != ',';
}

}  // namespace

void LineReceiver::begin() {
  serial_.setRxBufferSize(config::kLinkRxBufferBytes);  // Precisa vir antes do begin()
  serial_.begin(config::kLinkBaud, SERIAL_8N1, config::kLinkRxPin, config::kLinkTxPin);
}

bool LineReceiver::readLine(ReceivedLine& line) {
  while (serial_.available() > 0) {
    const char c = static_cast<char>(serial_.read());

    if (c == '\n') {
      const bool wasSynchronized = synchronized_;
      synchronized_ = true;
      if (wasSynchronized && finishLine(line)) {
        return true;
      }
      continue;
    }

    if (!synchronized_) {
      continue;
    }
    if (length_ < config::kLineBytes - 1) {
      buffer_[length_++] = c;
    } else {
      overflowed_ = true;
    }
  }
  return false;
}

bool LineReceiver::finishLine(ReceivedLine& line) {
  uint16_t length = length_;
  const bool overflowed = overflowed_;
  length_ = 0;
  overflowed_ = false;

  if (overflowed) {
    ++stats_.tooLong;
    return false;
  }

  // Os logs do TTeC usam println, que termina em "\r\n"
  if (length > 0 && buffer_[length - 1] == '\r') {
    --length;
  }
  buffer_[length] = '\0';

  bool valid = length > 0 && isPrintableAscii(buffer_, length);
  LineType type = LineType::kLog;
  if (valid) {
    if (buffer_[0] == '#') {
      type = LineType::kLog;
    } else if (strcmp(buffer_, config::kCsvHeader) == 0) {
      type = LineType::kHeader;
    } else if (isDataLine(buffer_, length)) {
      type = LineType::kData;
    } else {
      valid = false;
    }
  }

  if (!valid) {
    ++stats_.malformed;
    return false;
  }

  line.type = type;
  line.length = length;
  memcpy(line.text, buffer_, length + 1);
  ++stats_.accepted;
  return true;
}
