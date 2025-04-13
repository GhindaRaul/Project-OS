#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>  // Include the time.h header for ctime

#define clue_length 50
#define id_length 4
#define name_length 50
#define Treasure_file "treasure.dat"
#define LOG_FILE "logged_hunt"

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
            str[i] = '_';  // Replace space with underscore
        }
    }
}

void check_for_directory(const char *hunt_ID) {
    struct stat st;

    // Check if the directory exists
    if (stat(hunt_ID, &st) == -1) {
        if (mkdir(hunt_ID, 0755) == -1) {
            perror("mkdir failed");
            exit(1);
        } else {
            printf("Created hunt directory: %s\n", hunt_ID);
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory.\n", hunt_ID);
        exit(1);
    }
}

int treasure_exists(const char *hunt_ID, const char *treasureID) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_ID, Treasure_file);

    // Open the treasure file in read-only mode
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        return 0;  // If file doesn't exist, return 0 (not found)
    }

    char buffer[1024];
    ssize_t read_bytes;
    int found = 0;

    // Read the file and search for the treasureID
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        char *line = buffer;
        while (line < buffer + read_bytes) {
            char treasureID_local[id_length];
            char user_name[name_length];
            float longitude;
            float latitude;
            char clue_text[clue_length];
            int value;

            // Extract all the treasure info (ID, user, longitude, latitude, clue, value) from the line
            int n = sscanf(line, "%s %s %f %f %s %d", treasureID_local, user_name, &longitude, &latitude, clue_text, &value);
            if (n == 6 && strcmp(treasureID_local, treasureID) == 0) {
                found = 1;
                break;
            }

            // Move to the next treasure in the file (skip over the current line)
            while (line < buffer + read_bytes && *line != '\n') {
                line++;
            }
            line++; // Move past the newline character
        }
        if (found) break;
    }

    close(fd);
    return found;  // Return 1 if the treasureID was found, otherwise 0
}

void add_treasure(const char *hunt_ID, Treasure *treasure) {
    // Replace spaces with underscores in user_name and clue_text
    replace_spaces_with_underscores(treasure->User_name);
    replace_spaces_with_underscores(treasure->Clue_text);

    // Check if the treasure already exists in the file
    if (treasure_exists(hunt_ID, treasure->treasureID)) {
        fprintf(stderr, "Treasure with ID %s already exists!\n", treasure->treasureID);
        return;
    }

    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_ID, Treasure_file);

    // Open the treasure file in read-write, create if doesn't exist, and append
    int fd = open(file_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    // Write treasureID followed by a space
    char treasure_data[512];  // Buffer to hold formatted data
    snprintf(treasure_data, sizeof(treasure_data), "%s ", treasure->treasureID);
    ssize_t written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write treasureID");
        close(fd);
        exit(1);
    }

    // Write user_name followed by a space
    snprintf(treasure_data, sizeof(treasure_data), "%s ", treasure->User_name);
    written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write user_name");
        close(fd);
        exit(1);
    }

    // Write longitude as string followed by a space
    snprintf(treasure_data, sizeof(treasure_data), "%f ", treasure->longitude);
    written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write longitude");
        close(fd);
        exit(1);
    }

    // Write latitude as string followed by a space
    snprintf(treasure_data, sizeof(treasure_data), "%f ", treasure->latitude);
    written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write latitude");
        close(fd);
        exit(1);
    }

    // Write clue_text followed by a space
    snprintf(treasure_data, sizeof(treasure_data), "%s ", treasure->Clue_text);
    written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write clue_text");
        close(fd);
        exit(1);
    }

    // Write value followed by a space
    snprintf(treasure_data, sizeof(treasure_data), "%d\n", treasure->value);
    written = write(fd, treasure_data, strlen(treasure_data));
    if (written != strlen(treasure_data)) {
        perror("Failed to write value");
        close(fd);
        exit(1);
    }

    // Close file after writing all elements
    close(fd);

    // Logging the action
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/%s", hunt_ID, LOG_FILE);

    fd = open(log_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Failed to open log file");
        exit(1);
    }

    // Create a log entry
    char log_entry[256];
    snprintf(log_entry, sizeof(log_entry), "Added treasure with ID %s by user %s\n", treasure->treasureID, treasure->User_name);
    ssize_t log_written = write(fd, log_entry, strlen(log_entry));
    if (log_written != strlen(log_entry)) {
        perror("Failed to write to log file");
        close(fd);
        exit(1);
    }

    // Add newline character after log entry
    written = write(fd, "\n", 1);
    if (written != 1) {
        perror("Failed to write newline character in log");
        close(fd);
        exit(1);
    }

    close(fd);
}

void list_treasures(const char *hunt_ID) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_ID, Treasure_file);

    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Failed to get file status");
        exit(1);
    }

    // Print Hunt name, file size, and last modification time
    printf("Hunt: %s\n", hunt_ID);
    printf("Total File Size: %ld bytes\n", file_stat.st_size);
    printf("Last Modification Time: %s", ctime(&file_stat.st_mtime)); // Now works fine with %s

    // Open the treasure file in read-only mode
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    char buffer[1024];
    ssize_t read_bytes;
    printf("\nTreasures in hunt %s:\n", hunt_ID);
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, read_bytes);  // Print the contents of the file
    }

    if (read_bytes == -1) {
        perror("Error reading from file");
    }

    close(fd);
}

void view_treasure(const char *hunt_ID, const char *treasureID) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s", hunt_ID, Treasure_file);

    // Open the treasure file in read-only mode
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open treasure file");
        exit(1);
    }

    char buffer[1024];
    ssize_t read_bytes;
    int found = 0;

    // Search for the specific treasureID
    while ((read_bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        char *line = buffer;
        while (line < buffer + read_bytes) {
            char treasureID_local[id_length];
            char user_name[name_length];
            float longitude;
            float latitude;
            char clue_text[clue_length];
            int value;

            int n = sscanf(line, "%s %s %f %f %s %d", treasureID_local, user_name, &longitude, &latitude, clue_text, &value);
            if (n == 6 && strcmp(treasureID_local, treasureID) == 0) {
                // Print the treasure details
                printf("Treasure Details:\n");
                printf("Treasure ID: %s\n", treasureID_local);
                printf("User: %s\n", user_name);
                printf("Longitude: %.4f\n", longitude);
                printf("Latitude: %.4f\n", latitude);
                printf("Clue: %s\n", clue_text);
                printf("Value: %d\n", value);
                found = 1;
                break;
            }

            while (line < buffer + read_bytes && *line != '\n') {
                line++;
            }
            line++; // Skip newline character
        }

        if (found) break;
    }

    if (!found) {
        printf("Treasure with ID %s not found.\n", treasureID);
    }

    close(fd);
}


int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s --add|--list|--view <arguments>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--add") == 0) {
        if (argc != 9) {
            fprintf(stderr, "Usage for --add: %s --add <hunt_ID> <treasure_ID> <user_name> <longitude> <latitude> <clue> <value>\n", argv[0]);
            return 1;
        }

        Treasure treasure;
        char *ends;

        snprintf(treasure.treasureID, sizeof(treasure.treasureID), "%s", argv[3]);
        snprintf(treasure.User_name, sizeof(treasure.User_name), "%s", argv[4]);
        sscanf(argv[5], "%f", &treasure.longitude);
        sscanf(argv[6], "%f", &treasure.latitude);
        snprintf(treasure.Clue_text, sizeof(treasure.Clue_text), "%s", argv[7]);
        sscanf(argv[8], "%d", &treasure.value);

        // Check if the directory exists, if not, create it
        check_for_directory(argv[2]);

        // Add the treasure to the specified hunt
        add_treasure(argv[2], &treasure);
    }
    else if (strcmp(argv[1], "--list") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage for --list: %s --list <hunt_ID>\n", argv[0]);
            return 1;
        }

        // List treasures in the specified hunt directory
        list_treasures(argv[2]);
    }
    else if (strcmp(argv[1], "--view") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Usage for --view: %s --view <hunt_ID> <treasure_ID>\n", argv[0]);
            return 1;
        }

        // View details of a specific treasure
        view_treasure(argv[2], argv[3]);
    }
    else {
        fprintf(stderr, "Unknown option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

