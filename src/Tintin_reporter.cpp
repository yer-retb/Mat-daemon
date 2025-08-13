#include "../include/Tintin_reporter.hpp"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

Tintin_reporter::Tintin_reporter()
{
    _logPath = "/var/log/matt_daemon/matt_daemon.log";
    ensureLogDirectoryExists();
    _logFile.open(_logPath, std::ios::app);
    if (!_logFile.is_open())
    {
        std::cerr << "Failed to open log file: " << _logPath << std::endl;
    }
}

Tintin_reporter::~Tintin_reporter()
{
    if (_logFile.is_open())
        _logFile.close();
}

void Tintin_reporter::ensureLogDirectoryExists()
{
    std::filesystem::create_directories("/var/log/matt_daemon");
}

std::string Tintin_reporter::getTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm *ptm = std::localtime(&now_time);
    std::ostringstream oss;
    oss << "[" << std::setfill('0')
        << std::setw(2) << ptm->tm_mday << "/"
        << std::setw(2) << ptm->tm_mon + 1 << "/"
        << ptm->tm_year + 1900 << " - "
        << std::setw(2) << ptm->tm_hour << ":"
        << std::setw(2) << ptm->tm_min << ":"
        << std::setw(2) << ptm->tm_sec << "]";
    return oss.str();
}

void Tintin_reporter::log(const std::string &level, const std::string &message)
{
    std::lock_guard<std::mutex> lock(_logMutex);
    rotateLogIfNeeded();
    if (_logFile.is_open())
    {
        _logFile << getTimestamp() << " [" << level << "] - " << message << std::endl;
        _logFile.flush(); // Ensure immediate write for rotation size checking
    }
}

void Tintin_reporter::info(const std::string &message)
{
    log("INFO", message);
}

void Tintin_reporter::error(const std::string &message)
{
    log("ERROR", message);
}

void Tintin_reporter::raw(const std::string &message)
{
    std::lock_guard<std::mutex> lock(_logMutex);
    if (_logFile.is_open())
        _logFile << message << std::endl;
}

void Tintin_reporter::message(const std::string &message)
{
    log("MESSAGE", message);
}

void Tintin_reporter::rotateLogIfNeeded()
{
    if (std::filesystem::exists(_logPath))
    {
        size_t fileSize = std::filesystem::file_size(_logPath);
        if (fileSize >= MAX_LOG_SIZE)
        {
            _logFile.close();
            
            std::string archivedName = generateArchivedName();
            std::filesystem::rename(_logPath, archivedName);
            
            reopenLogFile();
            
            // Log rotation message AFTER reopening the file
            if (_logFile.is_open())
            {
                _logFile << getTimestamp() << " [INFO] - Matt_daemon: Log rotated. Archived as: " + archivedName << std::endl;
                _logFile.flush();
            }
            
            // Optionally compress the archived file
            std::string compressCmd = "gzip " + archivedName + " 2>/dev/null &";
            system(compressCmd.c_str());
        }
    }
}

std::string Tintin_reporter::generateArchivedName() const
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm *ptm = std::localtime(&now_time);

    std::ostringstream oss;
    oss << "/var/log/matt_daemon/matt_daemon_"
        << std::setfill('0')
        << std::setw(4) << ptm->tm_year + 1900
        << std::setw(2) << ptm->tm_mon + 1
        << std::setw(2) << ptm->tm_mday << "_"
        << std::setw(2) << ptm->tm_hour
        << std::setw(2) << ptm->tm_min << ".log";

    return oss.str();
}

void Tintin_reporter::reopenLogFile()
{
    _logFile.open(_logPath, std::ios::app);
    if (!_logFile.is_open())
    {
        std::cerr << "Failed to reopen log file: " << _logPath << std::endl;
    }
}
