#include "System.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sys/resource.h>

#include "utils/Logger.hpp"

namespace SystemResourceMonitor {

// ============================================================================
// Timer
// ============================================================================

std::chrono::steady_clock::time_point Timer::s_processStartTime =
  std::chrono::steady_clock::now();

Timer::Timer()
  : m_startTime(std::chrono::steady_clock::now())
{
}

std::chrono::microseconds
Timer::getRelativeTimeMicro() const
{
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(now -
                                                               m_startTime);
}

std::chrono::microseconds
Timer::getProcessRelativeTimeMicro()
{
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
    now - s_processStartTime);
}

std::chrono::microseconds
Timer::getAbsoluteTimeMicro()
{
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
    now.time_since_epoch());
}

void
Timer::reset()
{
  m_startTime = std::chrono::steady_clock::now();
}

// ============================================================================
// Memory
// ============================================================================

std::atomic<rlim_t> memoryLimitKB{ RLIM_INFINITY };

bool
parseMemInfo(const std::string& key, long& value)
{
  std::ifstream meminfo_file("/proc/meminfo");
  if (!meminfo_file.is_open()) {
    LOGERROR("Failed to open /proc/meminfo");
    return false;
  }

  std::string line;
  while (std::getline(meminfo_file, line)) {
    if (line.compare(0, key.length(), key) == 0) {
      std::string::size_type colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::istringstream iss(line.substr(colon_pos + 1));
        if (iss >> value) {
          return true;
        }
      }
      break;
    }
  }

  LOGERROR("Failed to find or parse key: %s", key.c_str());
  return false;
}

bool
parseProcStatus(const std::string& key, long& value)
{
  std::ifstream status_file("/proc/self/status");
  if (!status_file.is_open()) {
    LOGERROR("Failed to open /proc/self/status");
    return false;
  }

  std::string line;
  while (std::getline(status_file, line)) {
    if (line.compare(0, key.length(), key) == 0) {
      std::string::size_type colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::istringstream iss(line.substr(colon_pos + 1));
        if (iss >> value) {
          return true;
        }
      }
      break;
    }
  }

  LOGERROR("Failed to find or parse key: %s in /proc/self/status", key.c_str());
  return false;
}

bool
parseProcIO(const std::string& key, long& value)
{
  std::ifstream io_file("/proc/self/io");
  if (!io_file.is_open()) {
    LOGERROR("Failed to open /proc/self/io");
    return false;
  }

  std::string line;
  while (std::getline(io_file, line)) {
    if (line.compare(0, key.length(), key) == 0) {
      std::string::size_type colon_pos = line.find(':');
      if (colon_pos != std::string::npos) {
        std::istringstream iss(line.substr(colon_pos + 1));
        if (iss >> value) {
          return true;
        }
      }
      break;
    }
  }

  LOGERROR("Failed to find or parse key: %s in /proc/self/io", key.c_str());
  return false;
}

long
getTotalMemoryKB()
{
  long totalKB = 0;
  if (!parseMemInfo("MemTotal", totalKB)) {
    LOGERROR("Failed to get total memory");
  }
  return totalKB;
}

long
getUsedMemoryKB()
{
  long totalKB = 0, availableKB = 0;
  if (parseMemInfo("MemTotal", totalKB) &&
      parseMemInfo("MemAvailable", availableKB)) {
    return totalKB - availableKB;
  }
  LOGERROR("Failed to calculate used memory");
  return 0;
}

long
getFreeMemoryKB()
{
  long freeKB = 0;
  if (!parseMemInfo("MemFree", freeKB)) {
    LOGERROR("Failed to get free memory");
  }
  return freeKB;
}

long
getAvailableMemoryKB()
{
  long availableKB = 0;
  if (!parseMemInfo("MemAvailable", availableKB)) {
    LOGERROR("Failed to get available memory");
  }
  return availableKB;
}

bool
setMemoryLimitKB(rlim_t limitInKB)
{
  long availableMemory = getAvailableMemoryKB();
  if (limitInKB > static_cast<rlim_t>(availableMemory)) {
    LOGERROR("Attempted to set memory limit higher than available memory");
    return false;
  }

  memoryLimitKB.store(limitInKB);
  struct rlimit rlim;
  rlim.rlim_max = limitInKB * 1024;     // Convert to bytes
  rlim.rlim_cur = rlim.rlim_max * 0.8f; // 80% of the hard limit
  if (setrlimit(RLIMIT_AS, &rlim) != 0) {
    LOGERROR("Failed to set memory limit: %s", std::strerror(errno));
    return false;
  }
  return true;
}

rlim_t
getMemoryLimitKB()
{
  return memoryLimitKB.load();
}

bool
isMemoryLimitExceeded()
{
  return getUsedMemoryKB() > static_cast<long>(memoryLimitKB.load());
}

void
printProcessResourceUsage()
{
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    std::cerr << "e Failed to get resource usage information." << std::endl;
    return;
  }

  // Calculate CPU usage percentages
  double total_time = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec +
                      (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;
  double user_percent =
    (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6) / total_time * 100;
  double sys_percent =
    (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6) / total_time * 100;

  // Get memory information
  long total_mem = getTotalMemoryKB();
  long max_used_mem = usage.ru_maxrss;
  double mem_percent = static_cast<double>(max_used_mem) / total_mem * 100;

  std::ostringstream oss;

  int width = Logger::getTerminalWidth();

  // Helper function to print a metric line
  auto printMetric = [&oss, width](const std::string& label,
                                   const std::string& value) {
    // Protect against negative length when label+value exceeds terminal width
    int dotsCount = width - static_cast<int>(label.size() + value.size());
    std::string dots = std::string(dotsCount > 0 ? dotsCount : 0, '.');
    oss << "c " << label << "\033[2m" << dots << "\033[0m" // Dim style
        << value << std::endl;
  };

  // Helper function to print a section header
  auto printSection = [&oss](const std::string& title) {
    oss << "\nc "
        << BOLD << CYAN << "[ " << title << " ]" << RESET << std::endl;
  };

  // Main header
  // + characters in lambda - 2 for the two '+'
  std::string title("c ---------[ PROCESS RESOURCE USAGE STATISTICS ]");
  int dashesCount = width - static_cast<int>(title.size());
  if (dashesCount > 0) {
    title.append(std::string(dashesCount, '-'));
  }

  oss << "\n" << title << RESET << std::endl;

  long threads = 0;
  // CPU Usage Section
  printSection("CPU Usage");
  printMetric("User Time",
              std::to_string(usage.ru_utime.tv_sec) + "." +
                std::to_string(usage.ru_utime.tv_usec) + "s (" +
                std::to_string(user_percent).substr(0, 5) + "%)");
  printMetric("System Time",
              std::to_string(usage.ru_stime.tv_sec) + "." +
                std::to_string(usage.ru_stime.tv_usec) + "s (" +
                std::to_string(sys_percent).substr(0, 5) + "%)");
  if (parseProcStatus("Thread", threads)) {
    printMetric("Threads", std::to_string(threads));
  }
  // Memory Usage Section
  printSection("Memory Usage");
  long vm_peak = 0, vm_hwm = 0, vm_stk = 0, vm_swap = 0, vm_size = 0,
       vm_rss = 0;

  if (parseProcStatus("VmRSS", vm_rss)) {
    printMetric(
      "Physical Memory Current",
      std::to_string(vm_rss) + " KB (" +
        std::to_string(static_cast<double>(vm_rss) / total_mem * 100) +
        "% of total)");
  }
  // Memory breakdown (very insightful!)
  long rss_anon = 0, rss_file = 0, rss_shmem = 0;
  if (parseProcStatus("RssAnon", rss_anon)) {
    printMetric("  Anonymous (heap/stack)", std::to_string(rss_anon) + " KB");
  }
  if (parseProcStatus("RssFile", rss_file)) {
    printMetric("  File-backed", std::to_string(rss_file) + " KB");
  }
  if (parseProcStatus("RssShmem", rss_shmem)) {
    printMetric("  Shared memory", std::to_string(rss_shmem) + " KB");
  }
  printMetric("Physical Memory Peak ",
              std::to_string(max_used_mem) + " KB (" +
                std::to_string(mem_percent).substr(0, 5) + "% of total)");
  if (parseProcStatus("VmSize", vm_size)) {
    printMetric("Virtual Memory Current", std::to_string(vm_size) + " KB");
  }
  if (parseProcStatus("VmPeak", vm_peak)) {
    printMetric("Virtual Memory Peak", std::to_string(vm_peak) + " KB");
  }
  if (parseProcStatus("VmSwap", vm_swap)) {
    printMetric("Swapped Memory", std::to_string(vm_swap) + " KB");
  }

  // Page Faults Section
  printSection("Page Faults");
  printMetric("Soft Page Faults (reclaims)", std::to_string(usage.ru_minflt));
  printMetric("Hard Page Faults", std::to_string(usage.ru_majflt));

  // I/O Operations Section
  printSection("I/O Operations");

  // Detailed I/O from /proc/self/io
  long rchar = 0, wchar = 0, syscr = 0, syscw = 0;
  long read_bytes = 0, write_bytes = 0, cancelled_write_bytes = 0;

  if (parseProcIO("rchar", rchar)) {
    printMetric("Characters Read (including cache)",
                std::to_string(rchar) + " bytes");
  }
  if (parseProcIO("wchar", wchar)) {
    printMetric("Characters Written (including cache)",
                std::to_string(wchar) + " bytes");
  }
  if (parseProcIO("syscr", syscr)) {
    printMetric("Read System Calls", std::to_string(syscr));
  }
  if (parseProcIO("syscw", syscw)) {
    printMetric("Write System Calls", std::to_string(syscw));
  }

  printMetric("Block Input Operations", std::to_string(usage.ru_inblock));
  printMetric("Block Output Operations", std::to_string(usage.ru_oublock));

  // Context Switches Section
  printSection("Context Switches");
  printMetric("Voluntary", std::to_string(usage.ru_nvcsw));
  printMetric("Involuntary", std::to_string(usage.ru_nivcsw));

  oss << std::endl;

  LOG0("\n%s", oss.str().c_str());
}
}