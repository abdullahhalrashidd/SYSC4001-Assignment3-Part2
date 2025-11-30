#ifndef SHARED_H
#define SHARED_H

#include <semaphore.h>

#define NUM_QUESTIONS 5
#define NUM_EXAMS 20
#define RUBRIC_LINE_LENGTH 10

typedef struct {
    char rubric[NUM_QUESTIONS][RUBRIC_LINE_LENGTH]; // The rubric
    int current_exam; // Current exam index
    int current_student[NUM_EXAMS]; // An array of up to 20 student numbers, one per exam
    int completed_questions[NUM_QUESTIONS]; // Status per question
    
    // For Part 2b, will not affect part 2a
    sem_t question_mutex; // protects completed_questions[]
    sem_t rubric_mutex; // protects rubric[] + rubric file
} Shared_Memory;

#endif