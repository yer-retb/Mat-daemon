// system includes
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <ctime>

// local includes
#include "./include/server.hpp"
#include "./include/Tintin_reporter.hpp"

const char *LOCK_FILE = "/var/lock/matt_daemon.lock";
int lock_fd = -1;

void debug(const std::string &msg)
{
    std::ofstream log("/tmp/debug.log", std::ios::app);
    if (!log.is_open())
        return;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "[%d/%m/%Y - %H:%M:%S]", std::localtime(&time));

    log << buf << " " << msg << std::endl;
    log.close();
}

void handle_signal(int signum)
{
    debug("Received signal " + std::to_string(signum));
    exit(0); // This will trigger atexit()!
}

void setup_signals()
{
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
}

void daemonize()
{
    pid_t pid;

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (setsid() < 0)
        exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_RDWR);   // stderr
}

void create_lock_file(Tintin_reporter &logger)
{
    lock_fd = open(LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    logger.info("Creating lock file: " + std::string(LOCK_FILE));
    if (lock_fd < 0)
    {
        logger.error("Can't open: " + std::string(LOCK_FILE) + " (already locked?)");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    std::string pid_str = std::to_string(pid) + "\n";
    write(lock_fd, pid_str.c_str(), pid_str.length());
}

void remove_lock_file(Tintin_reporter &logger)
{
    logger.info("Removing lock file: " + std::string(LOCK_FILE));
    if (lock_fd >= 0)
    {
        close(lock_fd);
        unlink(LOCK_FILE);
    }
}

Tintin_reporter *global_logger = nullptr;

void cleanup_lock_file()
{
    if (global_logger)
        remove_lock_file(*global_logger);
}

int main()
{

    Tintin_reporter logger;
    global_logger = &logger;
    
    if (geteuid() != 0)
    {
        std::cerr << "Matt_daemon must be run as root!" << std::endl;
        exit(EXIT_FAILURE);
    }
    
    logger.info("Starting daemon setup...");

    daemonize();
    logger.info("Daemonized successfully");

    create_lock_file(logger);
    logger.info("Lock file created");

    setup_signals();

    atexit(cleanup_lock_file);

    run_server();

    return 0;
}
