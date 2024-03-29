#pragma once

#include <fstream>
#include <mutex>
#include <string>

#include "../include/log.h"
#include "../include/result.h"

namespace qosrtp {
/*
 * Automatically assign file paths and file names and prevent duplicate names;
 */
class LogFile {
 public:
  LogFile();
  ~LogFile();
  std::unique_ptr<Result> Write(const std::string& log_string);

 private:
  std::string GetProcessId();
  std::string GenerateRandomID(uint32_t length);
  bool ShouldChangeLogFile();
  std::unique_ptr<Result> ChangeLogFile();
  bool IsFileNameDuplicate(const std::string& log_path,
                           const std::string& log_file_name);
  // %rn - From "Rename Number", a field added to prevent duplicate names
  // between files. (_%rn) field will not be added when there is no duplicate
  // name
  static constexpr char kFileNameFormat[] = "qosrtp_%pid_%index(_%rn).log";
  static constexpr char kRelativeDirPath[] = "qosrtp_log";
  static constexpr uint32_t kRnLength = 8;
  static constexpr uint64_t kMaxFileSize = 50 * 1024 * 1024;
  std::mutex mutex_;
  uint32_t index_;
  std::string pid_;
  std::ofstream file_;
  std::string file_path_abs_;
};

class QosrtpLoggerDefault : public QosrtpLogger {
 public:
  QosrtpLoggerDefault(Level min_log_level);
  virtual ~QosrtpLoggerDefault() override;
  virtual void Log(Level log_level, const char* format, ...);

 private:
  std::unique_ptr<Result> FormatString(std::string& out_str, const char* format,
                                       va_list args);
  std::string BuildLogString(Level log_level, const std::string& message);
  std::string DateString();
  std::string LevelString(Level log_level);
  std::string ThreadString();
  static constexpr char kFormat[] = "[%date][%level][%thread] %message";
  // Max length of all parts of each item.
  static constexpr uint64_t kMaxItemLength = 64 * 1024;
  std::unique_ptr<LogFile> file_;
};
}  // namespace qosrtp