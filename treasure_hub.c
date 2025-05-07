// treasure_hub.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define CMD_FILE "monitor_cmd.txt"
#define ARG_FILE "monitor_args.txt"
#define MAX_INPUT_SIZE 256


pid_t monitor_pid = -1;
int monitor_running = 0;

void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
        write(STDOUT_FILENO, "Monitor exited\n", 15);
        monitor_running = 0;
        monitor_pid = -1;
    }
}

void send_command(const char *cmd, const char *args) {
    if (!monitor_running) {
        write(STDOUT_FILENO, "Monitor not running.\n", 20);
        return;
    }

    // Write the command to CMD_FILE
    int cmd_fd = open(CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cmd_fd == -1) {
        perror("Error opening CMD_FILE");
        return;
    }
    write(cmd_fd, cmd, strlen(cmd));
    close(cmd_fd);

    // Write the arguments to ARG_FILE (if any)
    if (args) {
        int arg_fd = open(ARG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (arg_fd == -1) {
            perror("Error opening ARG_FILE");
            return;
        }
        write(arg_fd, args, strlen(args));
        close(arg_fd);
    } else {
        // Ensure ARG_FILE is cleared if not used
        int arg_fd = open(ARG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (arg_fd != -1) {
            close(arg_fd);
        }
    }

    // Signal the monitor to process the command
    kill(monitor_pid, SIGUSR1);
}

void start_monitor() {
    if (monitor_running) {
        write(STDOUT_FILENO, "Monitor is already running.\n", 26);
        return;
    }

    monitor_pid = fork();
    if (monitor_pid == 0) {
        execl("./monitor", "monitor", NULL);
        perror("Failed to start monitor");
        exit(1);
    } else if (monitor_pid > 0) {
        write(STDOUT_FILENO, "Started monitor.\n", 17);
        monitor_running = 1;
    } else {
        perror("fork failed");
    }
}

void stop_monitor() {
    if (!monitor_running) {
        write(STDOUT_FILENO, "Monitor is not running.\n", 24);
        return;
    }

    send_command("stop", NULL);
    write(STDOUT_FILENO, "Sent stop command. Waiting for monitor to terminate...\n", 55);
}

// --- Command-specific functions ---

void list_hunts() {
    send_command("list_hunts", NULL);
}

void list_treasures(const char *args) {
    if (!args || strlen(args) == 0) {
        write(STDOUT_FILENO, "Usage: list_treasures <hunt_id>\n", 31);
        return;
    }
    send_command("list_treasures", args);
}

void view_treasure(const char *args) {
    if (!args || strlen(args) == 0) {
        write(STDOUT_FILENO, "Usage: view_treasure <hunt_id> <treasure_id>\n", 44);
        return;
    }
    send_command("view_treasure", args);
}

// --- Main interactive loop ---

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[MAX_INPUT_SIZE];
    ssize_t bytes_read;

    while (1) {
        // Prompt user for input
        write(STDOUT_FILENO, "treasure_hub> ", 14);
        bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);

        if (bytes_read <= 0) break;

        input[bytes_read - 1] = '\0';  // Replace newline with null terminator

        if (strcmp(input, "start_monitor") == 0) {
            start_monitor();
        } else if (strcmp(input, "list_hunts") == 0) {
            list_hunts();
        } else if (strncmp(input, "list_treasures", 14) == 0) {
            char *args = input + 15;
            list_treasures(args);
        } else if (strncmp(input, "view_treasure", 13) == 0) {
            char *args = input + 14;
            view_treasure(args);
        } else if (strcmp(input, "stop_monitor") == 0) {
            stop_monitor();
        } else if (strcmp(input, "exit") == 0) {
            if (monitor_running) {
                write(STDOUT_FILENO, "Monitor still running. Stop it before exiting.\n", 48);
            } else {
                break;
            }
        } else {
            write(STDOUT_FILENO, "Unknown command\n", 17);
        }
    }

    return 0;
}
