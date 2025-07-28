#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <fstream>
#include <mutex>

class Tintin_reporter {
private:
    std::ofstream _logFile;
    std::mutex _logMutex;
    std::string _logPath;

    std::string getTimestamp() const;
    void ensureLogDirectoryExists();

public:
    Tintin_reporter();
    ~Tintin_reporter();

    void log(const std::string &level, const std::string &message);

    void info(const std::string &message);
    void error(const std::string &message);
    void raw(const std::string &message);
    void message(const std::string &message);
};

#endif
