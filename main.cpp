#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <cstdlib>


const char* LOCK_FILE = "/var/lock/matt_daemon.lock";
int lock_fd = -1;

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
    // close(STDIN_FILENO);
    // close(STDOUT_FILENO);
    // close(STDERR_FILENO);
    
    // Optional: Redirect to /dev/null
    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_RDWR);   // stderr
}

void create_lock_file()
{
    lock_fd = open(LOCK_FILE, O_CREAT | O_EXCL | O_WRONLY, 0644);
    std::cout << "Creating lock file: " << LOCK_FILE << std::endl;
    if (lock_fd < 0)
    {
        std::cerr << "Can't open: " << LOCK_FILE << " (already locked?)" << std::endl;
        exit(EXIT_FAILURE);
    }

    pid_t pid = getpid();
    std::string pid_str = std::to_string(pid) + "\n";
    write(lock_fd, pid_str.c_str(), pid_str.length());
}

void remove_lock_file()
{
    std::cout << "Removing lock file: " << LOCK_FILE << std::endl;
    if (lock_fd >= 0)
    {
        close(lock_fd);
        unlink(LOCK_FILE);
    }
}


int main()
{
    std::cout << "Starting daemon..." << std::endl;
    daemonize();
    std::cout << "Daemon started with PID: " << std::endl;
    create_lock_file();
    std::cout << "Lock file created." << std::endl;
    atexit(remove_lock_file);
    std::cout << "Daemon is running..." << std::endl;
    
    while (true) {
        sleep(10);
    }
    
    return 0;
}
