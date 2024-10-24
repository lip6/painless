#include "System.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/resource.h>

#include "utils/Logger.hpp"

namespace SystemResourceMonitor {
std::atomic<rlim_t> memoryLimitKB{ RLIM_INFINITY };
const std::chrono::steady_clock::time_point startTime{ std::chrono::steady_clock::now() };

double
getRelativeTimeSeconds()
{
	auto now = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(now - startTime).count();
}

double
getAbsoluteTimeSeconds()
{
	auto now = std::chrono::system_clock::now();
	return std::chrono::duration<double>(now.time_since_epoch()).count();
}

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
	if (parseMemInfo("MemTotal", totalKB) && parseMemInfo("MemAvailable", availableKB)) {
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
	rlim.rlim_max = limitInKB * 1024; // Convert to bytes
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
		std::cerr << RED << BOLD << "Failed to get resource usage information." << RESET << std::endl;
		return;
	}

	// Calculate CPU usage percentages
	double total_time =
		usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;
	double user_percent = (usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6) / total_time * 100;
	double sys_percent = (usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6) / total_time * 100;

	// Get memory information
	long total_mem = getTotalMemoryKB();
	long max_used_mem = usage.ru_maxrss;
	double mem_percent = static_cast<double>(max_used_mem) / total_mem * 100;

	// Print header
	std::cout << CYAN << BOLD << std::setfill('=') << std::setw(80) << "" << RESET << std::endl;
	std::cout << CYAN << BOLD << "Process Resource Usage" << RESET << std::endl;
	std::cout << CYAN << BOLD << std::setfill('=') << std::setw(80) << "" << RESET << std::endl;
	std::cout << std::setfill(' ');

	// Function to print a row
	auto printRow = [](const std::string& label, const std::string& value, const std::string& color = WHITE) {
		std::cout << BOLD << BLUE << std::left << std::setw(40) << label << RESET << color << std::right
				  << std::setw(40) << value << RESET << std::endl;
	};

	// Print CPU usage
	printRow("CPU Time (User):",
			 std::to_string(usage.ru_utime.tv_sec) + "." + std::to_string(usage.ru_utime.tv_usec) + "s (" +
				 std::to_string(user_percent).substr(0, 5) + "%)",
			 GREEN);
	printRow("CPU Time (System):",
			 std::to_string(usage.ru_stime.tv_sec) + "." + std::to_string(usage.ru_stime.tv_usec) + "s (" +
				 std::to_string(sys_percent).substr(0, 5) + "%)",
			 YELLOW);

	// Print memory usage
	printRow("Maximum Resident Set Size:",
			 std::to_string(max_used_mem) + " KB (" + std::to_string(mem_percent).substr(0, 5) + "% of total)",
			 MAGENTA);

	// Print page faults
	printRow("Page Reclaims (Soft Page Faults):", std::to_string(usage.ru_minflt), CYAN);
	printRow("Page Faults (Hard Page Faults):", std::to_string(usage.ru_majflt), CYAN);

	// Print I/O operations
	printRow("Block Input Operations:", std::to_string(usage.ru_inblock), WHITE);
	printRow("Block Output Operations:", std::to_string(usage.ru_oublock), WHITE);

	// Print context switches
	printRow("Voluntary Context Switches:", std::to_string(usage.ru_nvcsw), GREEN);
	printRow("Involuntary Context Switches:", std::to_string(usage.ru_nivcsw), YELLOW);

	// Print footer
	std::cout << CYAN << BOLD << std::setfill('=') << std::setw(80) << "" << RESET << std::endl;
}
}