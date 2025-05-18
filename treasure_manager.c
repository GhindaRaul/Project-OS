#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>  // Included for PATH_MAX

#define clue_length 50
#define id_length 10
#define name_length 50
#define Treasure_file "treasure.dat"
#define LOG_FILE "logged_hunt"
#define PATH_MAX 4096

typedef struct treasure_manager {
    char treasureID[id_length];
    char User_name[name_length];
    float longitude;
    float latitude;
    char Clue_text[clue_length];
    int value;
} Treasure;

void replace_spaces_with_underscores(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ') {
            str[i] = '_';
        }
    }
}

void check_for_directory(const char *hunt_ID) {
    struct stat st;

    if (stat("hunts", &st) == -1) {
        if (mkdir("hunts", 0755) == -1) {
            perror("mkdir hunts failed");
            exit(1);
        }
    }

    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "hunts/%s", hunt_ID);

    if (stat(full_path, &st) == -1) {
        if (mkdir(full_path, 0755) == -1) {
            perror("mkdir hunt failed");
            exit(1);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory.\n", full_path);
        exit(1);
    }
}

int parse_treasure_line(const char *line, Treasure *t) {
    char tmp_line[256];
    strncpy(tmp_line, line, sizeof(tmp_line));
    tmp_line[sizeof(tmp_line) - 1] = 0;

    char *tokens[6];
    int count = 0;
    char *token = strtok(tmp_line, " \t\n");
    while (token && count < 6) {
        tokens[count++] = token;
        token = strtok(NULL, " \t\n");
    }
    if (count != 6) return 0;

    strncpy(t->treasureID, tokens[0], id_length - 1);
    t->treasureID[id_length - 1] = 0;

    strncpy(t->User_name, tokens[1], name_length - 1);
    t->User_name[name_length - 1] = 0;

    t->longitude = atof(tokens[2]);
    t->latitude = atof(tokens[3]);

    strncpy(t->Clue_text, tokens[4], clue_length - 1);
    t->Clue_text[clue_length - 1] = 0;

    t->value = atoi(tokens[5]);

    return 1;
}

int treasure_exists(const char *hunt_ID, const char *treasureID) {
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_ID, Treasure_file);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) return 0;

    char buf[1024], linebuf[256];
    ssize_t nread;
    int linepos = 0, found = 0;

    while ((nread = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < nread; i++) {
            if (buf[i] == '\n') {
                linebuf[linepos] = 0;
                Treasure t;
                if (parse_treasure_line(linebuf, &t) && strcmp(t.treasureID, treasureID) == 0) {
                    found = 1;
                    break;
                }
                linepos = 0;
            } else if (linepos < sizeof(linebuf) - 1) {
                linebuf[linepos++] = buf[i];
            }
        }
        if (found) break;
    }

    close(fd);
    return found;
}

void add_treasure(const char *hunt_ID, Treasure *treasure) {
    replace_spaces_with_underscores(treasure->User_name);
    replace_spaces_with_underscores(treasure->Clue_text);

    if (treasure_exists(hunt_ID, treasure->treasureID)) {
        write(STDERR_FILENO, "Treasure with this ID already exists!\n", 38);
        return;
    }

    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_ID, Treasure_file);

    int fd = open(file_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    char line[256];
    int len = snprintf(line, sizeof(line), "%s %s %f %f %s %d\n",
                       treasure->treasureID,
                       treasure->User_name,
                       treasure->longitude,
                       treasure->latitude,
                       treasure->Clue_text,
                       treasure->value);

    if (write(fd, line, len) != len) {
        perror("Failed to write treasure line");
    }
    close(fd);

    char log_path[PATH_MAX];
    snprintf(log_path, sizeof(log_path), "hunts/%s/%s", hunt_ID, LOG_FILE);
    fd = open(log_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open log file");
        exit(1);
    }

    len = snprintf(line, sizeof(line), "Added treasure with ID %s by user %s\n",
                   treasure->treasureID, treasure->User_name);
    if (write(fd, line, len) != len) {
        perror("Failed to write to log file");
    }
    close(fd);
}

void list_treasures(const char *hunt_ID) {
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_ID, Treasure_file);

    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Failed to get file status");
        exit(1);
    }

    dprintf(STDOUT_FILENO, "Hunt: %s\nTotal File Size: %ld bytes\nLast Modification Time: %s",
            hunt_ID, file_stat.st_size, ctime(&file_stat.st_mtime));

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    char buffer[1024];
    ssize_t read_bytes;
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        if (write(STDOUT_FILENO, buffer, read_bytes) != read_bytes) {
            perror("Failed to write to stdout");
        }
    }

    close(fd);
}

void view_treasure(const char *hunt_ID, const char *treasureID) {
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_ID, Treasure_file);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    char buf[1024], linebuf[256];
    ssize_t nread;
    int linepos = 0, found = 0;

    while ((nread = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < nread; i++) {
            if (buf[i] == '\n') {
                linebuf[linepos] = 0;
                Treasure t;
                if (parse_treasure_line(linebuf, &t) && strcmp(t.treasureID, treasureID) == 0) {
                    dprintf(STDOUT_FILENO, "Treasure Details:\n");
                    dprintf(STDOUT_FILENO, "Treasure ID: %s\n", t.treasureID);
                    dprintf(STDOUT_FILENO, "User: %s\n", t.User_name);
                    dprintf(STDOUT_FILENO, "Longitude: %.4f\n", t.longitude);
                    dprintf(STDOUT_FILENO, "Latitude: %.4f\n", t.latitude);
                    dprintf(STDOUT_FILENO, "Clue: %s\n", t.Clue_text);
                    dprintf(STDOUT_FILENO, "Value: %d\n", t.value);
                    found = 1;
                    break;
                }
                linepos = 0;
            } else if (linepos < sizeof(linebuf) - 1) {
                linebuf[linepos++] = buf[i];
            }
        }
        if (found) break;
    }

    if (!found) {
        dprintf(STDOUT_FILENO, "Treasure with ID %s not found.\n", treasureID);
    }

    close(fd);
}

void remove_treasure(const char *hunt_id, const char *treasure_id) {
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_id, Treasure_file);

    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    Treasure *all_treasures = NULL;
    int total = 0;
    Treasure t;

    char buf[1024], line[256];
    ssize_t nread;
    int linepos = 0;

    while ((nread = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < nread; i++) {
            if (buf[i] == '\n') {
                line[linepos] = '\0';
                if (parse_treasure_line(line, &t)) {
                    Treasure *new_block = realloc(all_treasures, (total + 1) * sizeof(Treasure));
                    if (!new_block) {
                        perror("realloc");
                        free(all_treasures);
                        close(fd);
                        return;
                    }
                    all_treasures = new_block;
                    all_treasures[total++] = t;
                }
                linepos = 0;
            } else if (linepos < sizeof(line) - 1) {
                line[linepos++] = buf[i];
            }
        }
    }
    close(fd);

    Treasure *filtered = malloc(total * sizeof(Treasure));
    if (!filtered) {
        perror("malloc");
        free(all_treasures);
        return;
    }

    int found = 0, new_count = 0;
    for (int i = 0; i < total; ++i) {
        if (strcmp(all_treasures[i].treasureID, treasure_id) == 0) {
            found = 1;
        } else {
            filtered[new_count++] = all_treasures[i];
        }
    }

    free(all_treasures);

    if (!found) {
        printf("Treasure ID %s not found.\n", treasure_id);
        free(filtered);
        return;
    }

    fd = open(file_path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        perror("open write");
        free(filtered);
        return;
    }

    for (int i = 0; i < new_count; ++i) {
        char line[256];
        int len = snprintf(line, sizeof(line), "%s %s %.6f %.6f %s %d\n",
                           filtered[i].treasureID,
                           filtered[i].User_name,
                           filtered[i].longitude,
                           filtered[i].latitude,
                           filtered[i].Clue_text,
                           filtered[i].value);
        if (write(fd, line, len) != len) {
            perror("write");
        }
    }

    close(fd);
    free(filtered);

    printf("Treasure removed.\n");
}

void remove_hunt(const char *hunt_id) {
    char file_path[PATH_MAX], log_path[PATH_MAX], dir_path[PATH_MAX];

    snprintf(file_path, sizeof(file_path), "hunts/%s/%s", hunt_id, Treasure_file);
    snprintf(log_path, sizeof(log_path), "hunts/%s/%s", hunt_id, LOG_FILE);
    snprintf(dir_path, sizeof(dir_path), "hunts/%s", hunt_id);

    unlink(file_path);
    unlink(log_path);
    rmdir(dir_path);

    printf("Hunt removed.\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        dprintf(STDERR_FILENO, "Usage: %s --add|--list|--view <arguments>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0) {
        if (argc != 9) {
            dprintf(STDERR_FILENO, "Usage for --add: %s --add <hunt_ID> <treasure_ID> <user_name> <longitude> <latitude> <clue> <value>\n", argv[0]);
            return 1;
        }

        Treasure treasure;
        snprintf(treasure.treasureID, sizeof(treasure.treasureID), "%s", argv[3]);
        snprintf(treasure.User_name, sizeof(treasure.User_name), "%s", argv[4]);
        treasure.longitude = atof(argv[5]);
        treasure.latitude = atof(argv[6]);
        snprintf(treasure.Clue_text, sizeof(treasure.Clue_text), "%s", argv[7]);
        treasure.value = atoi(argv[8]);

        check_for_directory(argv[2]);
        add_treasure(argv[2], &treasure);
    }
    else if (strcmp(argv[1], "--list") == 0) {
        if (argc != 3) {
            dprintf(STDERR_FILENO, "Usage for --list: %s --list <hunt_ID>\n", argv[0]);
            return 1;
        }
        list_treasures(argv[2]);
    }
    else if (strcmp(argv[1], "--view") == 0) {
        if (argc != 4) {
            dprintf(STDERR_FILENO, "Usage for --view: %s --view <hunt_ID> <treasure_ID>\n", argv[0]);
            return 1;
        }
        view_treasure(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "--remove") == 0) {
        if (argc != 4) {
            dprintf(STDERR_FILENO, "Usage for --remove: %s --remove <hunt_ID> <treasure_ID>\n", argv[0]);
            return 1;
        }
        remove_treasure(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "--delete-hunt") == 0) {
        if (argc != 3) {
            dprintf(STDERR_FILENO, "Usage for --delete-hunt: %s --delete-hunt <hunt_ID>\n", argv[0]);
            return 1;
        }
        remove_hunt(argv[2]);
    }
    else {
        dprintf(STDERR_FILENO, "Unknown option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
