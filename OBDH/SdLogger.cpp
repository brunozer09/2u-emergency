#include "SdLogger.h"

#include <SD.h>
#include <SPI.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ProjectConfig.h"

namespace {
constexpr size_t kPathBytes = 32;
constexpr char kNewline = '\n';

void buildPath(uint16_t index, char* path, size_t pathBytes) {
  snprintf(path, pathBytes, "%s/LOG%04u.CSV", config::kSdMountPoint,
           static_cast<unsigned int>(index));
}

bool fileExists(const char* path) {
  struct stat info;
  return stat(path, &info) == 0;
}

bool writeText(FILE* file, const char* text, size_t length) {
  return fwrite(text, 1, length, file) == length && fputc(kNewline, file) == kNewline;
}
}  // namespace

bool SdLogger::begin() {
  SD.end();
  SPI.begin(config::kSdSckPin, config::kSdMisoPin, config::kSdMosiPin, config::kSdCsPin);
  stats_.ready = SD.begin(config::kSdCsPin, SPI, config::kSdClockHz, config::kSdMountPoint,
                          config::kSdMaxOpenFiles);
  if (!stats_.ready) {
    ++stats_.mountFailures;
  }
  return stats_.ready;
}

bool SdLogger::write(const ReceivedLine& line) {
  // Cabeçalho repetido indica que o TTeC reiniciou: a nova execução vai para outro arquivo
  if (line.type == LineType::kHeader && file_ != nullptr && rowsInFile_ > 0) {
    closeFile();
  }

  if (file_ == nullptr && !openNextFile()) {
    handleFailure();
    return false;
  }

  // O arquivo já recebeu o cabeçalho ao ser aberto
  if (line.type == LineType::kHeader) {
    return true;
  }

  if (!writeText(file_, line.text, line.length)) {
    handleFailure();
    return false;
  }

  if (line.type == LineType::kData) {
    ++rowsInFile_;
  }
  ++stats_.linesWritten;
  dirty_ = true;
  return true;
}

void SdLogger::flushIfDue() {
  if (file_ == nullptr || !dirty_ || millis() - lastFlushMs_ < config::kSdFlushPeriodMs) {
    return;
  }

  if (fflush(file_) != 0 || fsync(fileno(file_)) != 0) {
    handleFailure();
    return;
  }
  dirty_ = false;
  lastFlushMs_ = millis();
}

bool SdLogger::openNextFile() {
  char path[kPathBytes];
  while (nextIndex_ <= config::kSdMaxFileIndex) {
    buildPath(nextIndex_, path, sizeof(path));
    if (!fileExists(path)) {
      break;
    }
    ++nextIndex_;
  }
  if (nextIndex_ > config::kSdMaxFileIndex) {
    return false;
  }

  file_ = fopen(path, "w");
  if (file_ == nullptr) {
    return false;
  }
  if (!writeText(file_, config::kCsvHeader, sizeof(config::kCsvHeader) - 1)) {
    closeFile();
    return false;
  }

  stats_.fileIndex = nextIndex_++;
  rowsInFile_ = 0;
  dirty_ = true;
  lastFlushMs_ = millis();
  return true;
}

void SdLogger::closeFile() {
  if (file_ != nullptr) {
    fclose(file_);
    file_ = nullptr;
  }
  dirty_ = false;
}

void SdLogger::handleFailure() {
  closeFile();
  SD.end();
  stats_.ready = false;
  ++stats_.failures;
}
