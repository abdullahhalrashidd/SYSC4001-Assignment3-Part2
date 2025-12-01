#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "shared_memory.h"

Shared_Memory* shared;

// Function to load rubric into shared memory 
void load_rubric(const char *rubric) {
    FILE *file = fopen(rubric, "r");
    if (file == NULL) {
        printf("Error: Could not open file.\n");
        exit(1); // Exit the program with an error code
    }
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        int j = 0;
        for (; j < RUBRIC_LINE_LENGTH - 1; j++) {
            int character = fgetc(file); // Convert each character to an int
            if (character == EOF) {
                if (j == 0) {
                    printf("Error: rubric file ended unexpectedly.\n"); // If there is nothing in the file
                    exit(1);
                } else {
                    break;   // treat EOF after some chars as end-of-line
                }
            }
            if (character == '\n') {
                break; // If the character is a new line character
            } else {
                shared->rubric[i][j] = (char)character; // Add into the 2D array
            }
        }
        shared->rubric[i][j] = '\0'; // Include the null character at the end of each row
    }
    fclose(file); // Close the file
}

// Function to load exams in shared memory
void load_exams(const char *exam) {
    FILE *file = fopen(exam, "r");
    if (file == NULL) {
        printf("Error: Could not open file.\n");
        exit(1); // Exit the program with an error code
    }
    int student_number;
    int buffer = fscanf(file, "%d", &student_number);
    if (buffer != 1) {
        printf("Error: could not read student number from exam file.\n");
        exit(1);
    }
    shared->current_student[shared->current_exam] = student_number;
    shared->current_exam++;

    fclose(file);
}

// Function to mark exams
void mark_exams(int TA_ID) {
    int exam_index = shared->current_exam - 1; // Current exam index in shared_memory array
    int current_student = shared->current_student[exam_index]; // Current student
    if (current_student == 9999) {
        printf("TA %d: Reached sentinel student 9999, stopping.\n", TA_ID);
        return;  // no marking for the sentinel
    }
    while (1) {
        int current_question = -1; // To keep track of which question is being marked currently
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (shared->completed_questions[i] == 0) {
                shared->completed_questions[i] = 1;
                current_question = i; // found a question to mark
                break;
            }
        }
        if (current_question == -1) {
            break;
        }
        printf("TA %d: Marking question %d for student %d.\n", TA_ID, current_question + 1, current_student);
        int delay = (rand() % 2) + 1;
        sleep(delay);
    }
}

// Function to handle rubric
void edit_rubric(int TA_ID, const char *rubric) {
    bool changed_any = false;
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("TA %d: Checking line %d of the rubric.\n", TA_ID, i+1);
        int line_to_change = rand() % 2;
        if (line_to_change == 1) { // If 1 change rubric
            changed_any = true; // Something in the rubric has been changed
            shared->rubric[i][3]++; // Increment character associated with number
            sleep(1); // 1 second delay to avoid super quick changing
            printf("TA %d: Line %d of the rubric has been changed.\n", TA_ID, i + 1);
        }
        int micros = (rand() % 500001) + 500000; // Delay between 0.5 s and 1.0 s
        usleep(micros); // Sleep
    }
    if (changed_any) { // If anything was changed, rewrite the rubric file
        FILE *file = fopen(rubric, "w");
        if (file == NULL) {
            printf("Error: could not open rubric file for writing.\n");
            exit(1);
        }
        for (int j = 0; j < NUM_QUESTIONS; j++) {
            fprintf(file, "%s\n", shared->rubric[j]); // Write changes in rubric file back to shared memory
        }
        fclose(file);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr,
                "Invalid arguments.\n"
                "Please run as: %s <number_of_TAs> <rubric_file> <exam_file_prefix>\n"
                "Example: %s 3 rubric.txt Exams/exam\n",
                argv[0], argv[0]);
        return 1; // Error has occured
    }

    int num_TAs = atoi(argv[1]);
    const char *rubfile = argv[2];
    const char *exam_prefix = argv[3]; // e.g. "Exams/exam"

    if (num_TAs < 2) {
        fprintf(stderr, "Error: <number_of_TAs> must be at least 2.\n");
        return 1;
    }

    // Create shared memory segment
    int shmid = shmget(IPC_PRIVATE, sizeof(Shared_Memory), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return 1;
    }

    // Attach shared memory and assign global pointer
    shared = (Shared_Memory *)shmat(shmid, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // Initialize shared memory fields
    shared->current_exam = 0;
    for (int i = 0; i < NUM_EXAMS; i++) {
        shared->current_student[i] = 0;
    }
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shared->completed_questions[i] = 0;
    }

    // Load rubric once at the start
    load_rubric(rubfile);

    // Loop over all exams in the "pile"
    // exam files: <prefix>01.txt, <prefix>02.txt, ..., up to NUM_EXAMS
    for (int exam_index = 1; exam_index <= NUM_EXAMS; exam_index++) {
        char exam_path[256];

        // build something like "Exams/exam01.txt"
        snprintf(exam_path, sizeof(exam_path),
                 "%s%02d.txt", exam_prefix, exam_index);

        // First, fork a loader TA to load this exam file
        pid_t loader_pid = fork();
        if (loader_pid < 0) {
            perror("fork");
            shmdt(shared);
            shmctl(shmid, IPC_RMID, NULL);
            return 1;
        }

        if (loader_pid == 0) {
            // Loader TA process (we'll just call this TA 1 for printing)
            int TA_ID = 1;
            printf("TA %d: loading exam file %s\n", TA_ID, exam_path);
            load_exams(exam_path);
            _exit(0);
        }

        // Parent: wait for the loader TA to finish loading the exam
        waitpid(loader_pid, NULL, 0);

        int student = shared->current_student[shared->current_exam - 1];
        if (student == 9999) {
            printf("TA 1: reached sentinel student 9999 in %s. Stopping.\n",
                   exam_path);
            break; // don't mark this exam; just stop
        }

        // reset question completion flags for this exam
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            shared->completed_questions[i] = 0;
        }

        // Fork TA processes to work on THIS exam
        for (int i = 0; i < num_TAs; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                // clean up and bail
                shmdt(shared);
                shmctl(shmid, IPC_RMID, NULL);
                return 1;
            }

            if (pid == 0) {
                // child process = TA (ID = i+1)
                srand(getpid()); // unique seed per TA process
                edit_rubric(i + 1, rubfile); // rubric phase
                mark_exams(i + 1); // marking phase
                _exit(0);
            }
            // parent continues to spawn other TAs
        }

        // Parent waits for all TA processes to finish this exam
        for (int i = 0; i < num_TAs; i++) {
            wait(NULL);
        }

        printf("TA 1: finished marking exam file %s\n", exam_path);
    }

    // Detach and remove shared memory
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
