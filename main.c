
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ALPHABET_SIZE 26
#define BUFFER_SIZE (ALPHABET_SIZE * sizeof(int))

// Structure for histogram data
typedef struct {
    int counts[ALPHABET_SIZE];
} Histogram;

// Global variable declarations
pid_t *childPIDs; // Track child PIDs dynamically
int nChildren = 0; // Number of child processes
int **pipes; // Pipes for communication dynamically allocated

void computeHistogram(const char *filename, Histogram *hist);
void writeHistogramToFile(const char *filename, const Histogram *hist);

void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    Histogram hist;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < nChildren; i++) {
            if (pid == childPIDs[i]) {
                // Read the histogram data from the corresponding pipe
                if (read(pipes[i][0], &hist, BUFFER_SIZE) == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }

                // Close the read end of the pipe
                close(pipes[i][0]);

                // Write histogram to file
                char filename[256];
                snprintf(filename, sizeof(filename), "file%ld.hist", (long)pid);
                writeHistogramToFile(filename, &hist);

                // Remove PID from tracking array (simplified; actual implementation may need to shift array elements)
                childPIDs[i] = -1;
                break;
            }
        }
        nChildren--;
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (argc < 2) {
        fprintf(stderr, "Error: No input files provided.\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory based on the number of input files
    nChildren = argc - 1;
    childPIDs = malloc(nChildren * sizeof(pid_t));
    pipes = malloc(nChildren * sizeof(int*));
    for (int i = 0; i < nChildren; i++) {
        pipes[i] = malloc(2 * sizeof(int));
    }

    for (int i = 1; i < argc; i++) {
        if (pipe(pipes[i-1]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            // Parent process
            close(pipes[i-1][1]); // Close the write end of the pipe
            childPIDs[i-1] = pid;
        } else {
            // Child process
            close(pipes[i-1][0]); // Close the read end of the pipe

            Histogram hist;
            memset(&hist, 0, sizeof(Histogram));

            if (strcmp(argv[i], "SIG") == 0) {
                // Simulate receiving SIGINT by exiting with a specific status
                exit(1);
            } else {
                computeHistogram(argv[i], &hist);
                if (write(pipes[i-1][1], &hist, BUFFER_SIZE) == -1) {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
                close(pipes[i-1][1]); // Close the write end after sending data
                exit(EXIT_SUCCESS);
            }
        }
    }

    // Wait for all children to exit
    while (nChildren > 0) {
        pause(); // Sleep until a signal is received
    }

    // Free allocated memory
    for (int i = 0; i < nChildren; i++) {
        free(pipes[i]);
    }
    free(pipes);
    free(childPIDs);

    return 0;
}


void computeHistogram(const char *filename, Histogram *hist) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    int c;
    while ((c = fgetc(file)) != EOF) {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; // Convert to lowercase
        if (c >= 'a' && c <= 'z') hist->counts[c - 'a']++;
    }
    fclose(file);
}

void writeHistogramToFile(const char *filename, const Histogram *hist) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        fprintf(file, "%c %d\n", 'a' + i, hist->counts[i]);
    }
    fclose(file);
}


