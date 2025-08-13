#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>

class Tintin_reporter
{
private:
    std::ofstream _logFile;
    std::mutex _logMutex;
    std::string _logPath;
    static const size_t MAX_LOG_SIZE = 10240; // 10KB

    std::string getTimestamp() const;
    void ensureLogDirectoryExists();
    void rotateLogIfNeeded();
    std::string generateArchivedName() const;
    void reopenLogFile();

public:
    Tintin_reporter();
    ~Tintin_reporter();
    // wa ziiid hna to do

    void log(const std::string &level, const std::string &message);

    void info(const std::string &message);
    void error(const std::string &message);
    void raw(const std::string &message);
    void message(const std::string &message);
};

#endif
