#include "log_default.h"

#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#if defined(_MSC_VER)
#include <windows.h>
#if defined(max)
#undef max
#endif  // max
#if defined(min)
#undef min
#endif  // min
#elif defined(QOSRTP_POSIX)
#include <unistd.h>
#else
#error "Unsupported compiler"
#endif

using namespace qosrtp;

LogFile::LogFile() : file_(), index_(0), file_path_abs_("") {
  pid_ = GetProcessId();
}

LogFile::~LogFile() {
  if (file_.is_open()) file_.close();
}

std::unique_ptr<Result> LogFile::Write(const std::string& log_string) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ShouldChangeLogFile()) {
    if (!ChangeLogFile()->ok()) {
      return Result::Create(-1, "Failed to change file.");
    }
  }
  file_ << log_string << std::endl;
  return Result::Create();
}

bool LogFile::ShouldChangeLogFile() {
  if (!file_.is_open()) {
    return true;
  }
  uint32_t size_file = std::filesystem::file_size(file_path_abs_);
  if (size_file > kMaxFileSize) {
    return true;
  }
  return false;
}

std::unique_ptr<Result> LogFile::ChangeLogFile() {
  std::string filename = kFileNameFormat;
  filename.replace(filename.find("%pid"), 4, pid_);
  filename.replace(filename.find("%index"), 6, std::to_string(index_));
  std::string filename_tmpt = filename;
  filename_tmpt.replace(filename_tmpt.find("(_%rn)"), 6, std::string(""));
  std::string bin_path = std::filesystem::current_path().string();
  std::string log_path = bin_path;
#if defined(_MSC_VER)
  log_path.append("\\");
#elif defined(QOSRTP_POSIX)
  log_path.append("/");
#else
#error "Unsupported compiler"
#endif
  log_path.append(kRelativeDirPath);
  if (!std::filesystem::is_directory(log_path)) {
    if (!std::filesystem::create_directories(log_path)) {
      return Result::Create(-1, "Failed to create folder.");
    }
  }
  while (IsFileNameDuplicate(log_path, filename_tmpt)) {
    filename_tmpt = filename;
    filename_tmpt.replace(filename_tmpt.find("(_%rn)"), 6,
                          GenerateRandomID(kRnLength));
  }
  filename = filename_tmpt;
  if (file_.is_open()) {
    file_.close();
  }
  std::string file_path_abs = log_path;
#if defined(_MSC_VER)
  file_path_abs.append("\\");
#elif defined(QOSRTP_POSIX)
  file_path_abs.append("/");
#else
#error "Unsupported compiler"
#endif
  file_path_abs.append(filename);
  file_.open(file_path_abs);
  if (!file_.is_open()) {
    return Result::Create(-1, "Failed to open file.");
  }
  file_path_abs_ = file_path_abs;
  index_++;
  return Result::Create();
}

bool LogFile::IsFileNameDuplicate(const std::string& log_path,
                                  const std::string& log_file_name) {
  bool has_duplicate_files = false;
  for (const auto& entry : std::filesystem::directory_iterator(log_path)) {
    std::string existing_file_name = entry.path().filename().string();
    if (log_file_name == existing_file_name) {
      has_duplicate_files = true;
      break;
    }
  }
  return has_duplicate_files;
}

std::string LogFile::GetProcessId() {
  int32_t num_pid = -1;
#if defined(_MSC_VER)
  num_pid = static_cast<int32_t>(GetCurrentProcessId());
#elif defined(QOSRTP_POSIX)
  num_pid = static_cast<int32_t>(getpid());
#else
#error "Unsupported compiler"
#endif
  return std::to_string(num_pid);
}

std::string LogFile::GenerateRandomID(uint32_t length) {
  const std::string charset =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> distribution(0, charset.size() - 1);
  std::string id;
  for (uint32_t i = 0; i < length; ++i) {
    id += charset[distribution(gen)];
  }
  return id;
}

QosrtpLoggerDefault::QosrtpLoggerDefault(Level min_log_level)
    : QosrtpLogger(min_log_level) {
  file_ = std::make_unique<LogFile>();
}

QosrtpLoggerDefault::~QosrtpLoggerDefault() = default;

void QosrtpLoggerDefault::Log(Level log_level, const char* format, ...) {
  if (log_level < min_log_level_ || log_level > Level::kFatal) {
    return;
  }
  va_list args;
  va_start(args, format);
  std::string str_message = "";
  std::unique_ptr<Result> message_result =
      QosrtpLoggerDefault::FormatString(str_message, format, args);
  va_end(args);
  if (!message_result->ok()) {
    return;
  }
  std::string output_log = BuildLogString(log_level, str_message);
  if (output_log.size() > kMaxItemLength) {
    output_log = output_log.substr(0, kMaxItemLength);
  }
  /*TODO 总长度，选择文件，写入*/
  if (!file_->Write(output_log)->ok()) {
    return;
  }
  return;
}

std::unique_ptr<Result> QosrtpLoggerDefault::FormatString(std::string& out_str,
                                                          const char* format,
                                                          va_list args) {
  int buffer_size = std::vsnprintf(nullptr, 0, format, args);
  if (buffer_size < 0) {
    return Result::Create(buffer_size, "Format error.");
  }
  std::unique_ptr<char[]> buffer(new char[buffer_size + 1]);
  int result = std::vsnprintf(buffer.get(), buffer_size + 1, format, args);
  if (result < 0) {
    return Result::Create(result, "Format error.");
  }
  out_str = std::string(buffer.get());
  return Result::Create();
}

std::string QosrtpLoggerDefault::BuildLogString(Level log_level,
                                                const std::string& message) {
  std::string output_log = kFormat;
  std::string str_date;
  std::string str_level;
  std::string str_thread;
  output_log.replace(output_log.find("%date"), 5, DateString());
  output_log.replace(output_log.find("%level"), 6, LevelString(log_level));
  output_log.replace(output_log.find("%thread"), 7, ThreadString());
  output_log.replace(output_log.find("%message"), 8, message);
  return std::move(output_log);
}

std::string QosrtpLoggerDefault::DateString() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm timeInfo = *std::localtime(&time);
  std::stringstream ss;
  ss << std::put_time(&timeInfo, "%Y-%m-%d-%H:%M:%S.");
  ss << std::setfill('0') << std::setw(3) << ms % 1000;
  return ss.str();
}

std::string QosrtpLoggerDefault::LevelString(Level log_level) {
  switch (log_level) {
    case Level::kTrace:
      return std::string("Trace");
      break;
    case Level::kInfo:
      return std::string("Info");
      break;
    case Level::kWarning:
      return std::string("Warning");
      break;
    case Level::kError:
      return std::string("Error");
      break;
    case Level::kFatal:
      return std::string("Fatal");
      break;
    default:
      assert(false);
      break;
  }
  return std::string("");
}

std::string QosrtpLoggerDefault::ThreadString() {
  std::ostringstream oss;
  std::thread::id thread_id = std::this_thread::get_id();
  oss << thread_id;
  return oss.str();
}