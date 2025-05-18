#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREASURE_FILE "treasure.dat"
#define MAX_USERNAME 32

typedef struct {
    char username[MAX_USERNAME];
    int total_score;
} ScoreEntry;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char *msg = "Usage: score_calculator <hunt_directory>\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return 1;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", argv[1], TREASURE_FILE);
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open treasure file");
        return 1;
    }

    ScoreEntry scores[100];
    int count = 0;

    char buffer[1024];
    ssize_t bytes_read;
    int buf_pos = 0;

    while ((bytes_read = read(fd, buffer + buf_pos, sizeof(buffer) - buf_pos - 1)) > 0) {
        buffer[buf_pos + bytes_read] = '\0';
        char *line = strtok(buffer, "\n");
        while (line) {
            char id[16], username[MAX_USERNAME], clue[128];
            float lat, lon;
            int value;

            if (sscanf(line, "%15s %31s %f %f %127s %d", id, username, &lat, &lon, clue, &value) == 6) {
                int found = 0;
                for (int i = 0; i < count; ++i) {
                    if (strcmp(scores[i].username, username) == 0) {
                        scores[i].total_score += value;
                        found = 1;
                        break;
                    }
                }
                if (!found && count < 100) {
                    strncpy(scores[count].username, username, MAX_USERNAME);
                    scores[count].total_score = value;
                    count++;
                }
            }

            line = strtok(NULL, "\n");
        }
        buf_pos = 0;  // reset after processing
    }

    close(fd);

    // Output scores using write()
    for (int i = 0; i < count; i++) {
        char output[64];
        int len = snprintf(output, sizeof(output), "%s %d\n", scores[i].username, scores[i].total_score);
        write(STDOUT_FILENO, output, len);
    }

    return 0;
}
