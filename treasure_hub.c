#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define CMD_FILE "monitor_cmd.txt"
#define ARG_FILE "monitor_args.txt"
#define MAX_INPUT_SIZE 256
#define MAX_BUFFER 1024

pid_t monitor_pid = -1;
int monitor_running = 0;
int monitor_pipe[2];

// --- Helper to clear command and argument files ---
void clear_command_files() {
    FILE *f = fopen(CMD_FILE, "w");
    if (f) fclose(f);
    f = fopen(ARG_FILE, "w");
    if (f) fclose(f);
}

// --- Monitor simulation logic ---
void simulate_monitor_loop() {
    signal(SIGUSR1, SIG_IGN);  // Replace with handler if needed

    while (1) {
        FILE *cmd_fp = fopen(CMD_FILE, "r");
        FILE *arg_fp = fopen(ARG_FILE, "r");
        char command[MAX_INPUT_SIZE] = {0};
        char args[MAX_INPUT_SIZE] = {0};

        if (cmd_fp) {
            fread(command, 1, sizeof(command) - 1, cmd_fp);
            command[strcspn(command, "\n")] = '\0';
            fclose(cmd_fp);
        }
        if (arg_fp) {
            fread(args, 1, sizeof(args) - 1, arg_fp);
            args[strcspn(args, "\n")] = '\0';
            fclose(arg_fp);
        }

        if (strcmp(command, "stop") == 0) {
            dprintf(STDOUT_FILENO, "[Monitor] Stopping monitor process.\n");
            fflush(stdout);
            break;
        } else if (strcmp(command, "list_hunts") == 0) {
            DIR *dir = opendir("hunts");
            if (dir) {
                struct dirent *entry;
                struct stat st;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;
                    char path[512];
                    snprintf(path, sizeof(path), "hunts/%s", entry->d_name);
                    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        dprintf(STDOUT_FILENO, "%s\n", entry->d_name);
                    }
                }
                closedir(dir);
            } else {
                dprintf(STDOUT_FILENO, "Error: Could not open hunts directory\n");
            }
            fflush(stdout);
        } else if (strncmp(command, "list_treasures", 14) == 0 && strlen(args) > 0) {
            char path[512];
            snprintf(path, sizeof(path), "./treasure_manager --list %s", args);
            dprintf(STDOUT_FILENO, "[Monitor] Listing treasures in %s\n", args);
            fflush(stdout);
            system(path);
        } else if (strncmp(command, "view_treasure", 13) == 0 && strlen(args) > 0) {
            char path[512];
            snprintf(path, sizeof(path), "./treasure_manager --view %s", args);
            dprintf(STDOUT_FILENO, "[Monitor] Viewing treasure: %s\n", args);
            fflush(stdout);
            system(path);
        } else if (strlen(command) > 0) {
            dprintf(STDOUT_FILENO, "[Monitor] Unknown command: %s\n", command);
            fflush(stdout);
        }

        // Clear command and argument files after processing
        FILE *cmd_clear = fopen(CMD_FILE, "w");
        if (cmd_clear) fclose(cmd_clear);
        FILE *arg_clear = fopen(ARG_FILE, "w");
        if (arg_clear) fclose(arg_clear);

        sleep(1);
    }
    exit(0);
}

// --- Signal handler for monitor child ---
void handle_sigchld(int sig) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid > 0) {
        write(STDOUT_FILENO, "Monitor exited\n", 15);
        monitor_running = 0;
        monitor_pid = -1;
    }
}

// --- Read from monitor pipe and print to stdout ---
#include <fcntl.h>

void read_from_monitor() {
    char buffer[MAX_BUFFER];
    ssize_t bytes_read;

    // Save current flags
    int flags = fcntl(monitor_pipe[0], F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get flags");
        return;
    }

    // Set pipe read end to non-blocking
    if (fcntl(monitor_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set non-blocking");
        return;
    }

    // Read whatever is available without blocking
    while ((bytes_read = read(monitor_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        write(STDOUT_FILENO, buffer, bytes_read);
    }

    // Restore original flags (blocking mode)
    if (fcntl(monitor_pipe[0], F_SETFL, flags) == -1) {
        perror("fcntl restore flags");
    }
}


// --- Send command to monitor via files and signal ---
void send_command(const char *cmd, const char *args) {
    if (!monitor_running) {
        write(STDOUT_FILENO, "Monitor not running.\n", 22);
        return;
    }

    int cmd_fd = open(CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (cmd_fd == -1) {
        perror("Error opening CMD_FILE");
        return;
    }
    write(cmd_fd, cmd, strlen(cmd));
    close(cmd_fd);

    int arg_fd = open(ARG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (arg_fd != -1) {
        if (args) {
            write(arg_fd, args, strlen(args));
        }
        close(arg_fd);
    }

    kill(monitor_pid, SIGUSR1);
    sleep(1);
    read_from_monitor();
}

// --- Start monitor process ---
void start_monitor() {
    if (monitor_running) {
        write(STDOUT_FILENO, "Monitor is already running.\n", 29);
        return;
    }

    clear_command_files();

    if (pipe(monitor_pipe) == -1) {
        perror("pipe failed");
        return;
    }

    monitor_pid = fork();
    if (monitor_pid == 0) {
        close(monitor_pipe[0]);
        dup2(monitor_pipe[1], STDOUT_FILENO);
        close(monitor_pipe[1]);
        simulate_monitor_loop();
        exit(0);
    } else if (monitor_pid > 0) {
        close(monitor_pipe[1]);
        write(STDOUT_FILENO, "Started monitor.\n", 17);
        monitor_running = 1;
    } else {
        perror("fork failed");
    }
}

// --- Stop monitor process ---
void stop_monitor() {
    if (!monitor_running) {
        write(STDOUT_FILENO, "Monitor is not running.\n", 25);
        return;
    }
    send_command("stop", NULL);
    write(STDOUT_FILENO, "Sent stop command.\n", 20);
}

// --- Command wrappers ---
void list_hunts() {
    send_command("list_hunts", NULL);
}

void list_treasures(const char *args) {
    if (!args || strlen(args) == 0) {
        write(STDOUT_FILENO, "Usage: list_treasures <hunt_id>\n", 33);
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

void calculate_scores() {
    DIR *dir = opendir("hunts");
    if (!dir) {
        perror("Could not open hunts directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip '.' and '..'
        if (entry->d_name[0] == '.') continue;

        int fd[2];
        if (pipe(fd) == -1) {
            perror("pipe");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(fd[0]);
            close(fd[1]);
            continue;
        }

        if (pid == 0) {
            // Child process
            close(fd[0]); // Close read end

            // Redirect stdout to pipe write end
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);

            // Build full path: hunts/Hunt01
            char hunt_path[512];
            int ret = snprintf(hunt_path, sizeof(hunt_path), "hunts/%s", entry->d_name);
            if (ret < 0 || ret >= (int)sizeof(hunt_path)) {
                fprintf(stderr, "Hunt path too long, skipping %s\n", entry->d_name);
                exit(1);
            }

            // Execute score_calculator with full hunt path
            execl("./score_calculator", "score_calculator", hunt_path, NULL);

            perror("execl score_calculator");
            exit(1);
        } else {
            // Parent process
            close(fd[1]); // Close write end

            printf("Scores for hunt %s:\n", entry->d_name);

            char buffer[512];
            ssize_t n;
            while ((n = read(fd[0], buffer, sizeof(buffer)-1)) > 0) {
                buffer[n] = '\0';
                printf("%s", buffer);
            }
            close(fd[0]);

            int status;
            waitpid(pid, &status, 0);
            printf("\n");
        }
    }

    closedir(dir);
}



// --- Main loop ---
int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    char input[MAX_INPUT_SIZE];
    ssize_t bytes_read;

    while (1) {
        write(STDOUT_FILENO, "treasure_hub> ", 14);
        bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
        if (bytes_read <= 0) break;

        if (input[bytes_read - 1] == '\n') {
            input[bytes_read - 1] = '\0';
        } else {
            input[bytes_read] = '\0';
        }

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
        } else if (strcmp(input, "calculate_score") == 0) {
            calculate_scores();
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
