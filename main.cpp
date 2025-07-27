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

    // Step 1: Fork
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS); // Parent exits

    // Step 2: Create new session
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // Step 3: Fork again
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // Step 4: Set file permissions mask
    umask(0);

    // Step 5: Change working directory
    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    // Step 6: Close file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Optional: Redirect to /dev/null
    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_RDWR);   // stderr
}

void create_lock_file()
{
    lock_fd = open(LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    debug("Creating lock file: " + std::string(LOCK_FILE));
    if (lock_fd < 0)
    {
        debug("Can't open: " + std::string(LOCK_FILE) + " (already locked?)");
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    std::string pid_str = std::to_string(pid) + "\n";
    write(lock_fd, pid_str.c_str(), pid_str.length());
}

void remove_lock_file()
{
    debug("Removing lock file: " + std::string(LOCK_FILE));
    if (lock_fd >= 0)
    {
        close(lock_fd);
        unlink(LOCK_FILE);
    }
}

int main()
{

    if (geteuid() != 0)
    {
        std::cerr << "Matt_daemon must be run as root!" << std::endl;
        exit(EXIT_FAILURE);
    }

    debug("Starting daemon setup...");
    
    daemonize();
    debug("Daemonized successfully");
    
    create_lock_file();
    debug("Lock file created");

    setup_signals();

    atexit(remove_lock_file);

    run_server();

    return 0;
}
