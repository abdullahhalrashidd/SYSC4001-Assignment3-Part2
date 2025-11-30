# SYSC 4001 – Assignment 3 Part 2

This repo contains my solution for Assignment 3 Part 2.

Programs:

- `Part2a_101310113_101256959.c` – version without semaphores.
- `Part2b_101310113_101256959.c` – version with semaphores.
- `shared_memory.h` – shared data structure definitions.

---

## How to compile

Run these on a Linux/Unix machine with `gcc`:

```bash
# Part 2a (no semaphores)
gcc -Wall -std=c11 Part2a_101310113_101256959.c -o part2a

# Part 2b (with semaphores)
gcc -Wall -std=c11 Part2b_101310113_101256959.c -o part2b -pthread
```

## Required input files

- `rubric.txt` – rubric file with one line per question (5 lines total). Example:

```text
1, A
2, B
3, C
4, D
5, E
```
- `Exams/exam01.txt` … `Exams/exam20.txt` – exam files.  

Each file contains **one 4-digit student number** on a single line. Example:

`0001`

The exam whose file contains `9999` is the **sentinel** exam: when that exam
is read, the program stops creating new work and terminates after finishing
the current exam.

## How to Run

Both programs use the same number of arguments:

```text
./program <num_TAs> <rubric_file> <exam_prefix>
```
- `<num_TAs>` – number of TA processes (i.e. 2, 3, 4)
- `<rubric_file>` – in this case is rubric.txt
- `<exam_prefix>` – i.e. exam01/exam02/exam18

Assuming your files are Exams/exam01.txt … Exams/exam20.txt:
```text
# Test 1: Part 2a with 2 TAs
./part2a 2 rubric.txt Exams/exam

# Test 2: Part 2a with 4 TAs
./part2a 4 rubric.txt Exams/exam

# Test 3: Part 2b with 2 TAs
./part2b 2 rubric.txt Exams/exam

# Test 4: Part 2b with 4 TAs
./part2b 4 rubric.txt Exams/exam
```

## Design vs Critical-section requirements

The three critical-section requirements are:

1. Mutual exclusion – at most one process in the critical section at a time.  
2. Progress – if no one is in the critical section, some waiting process can enter.  
3. Bounded waiting – a process is not forced to wait forever before entering.

### Part 2a (no semaphores)

`Part2a_101310113_101256959.c` does not use any semaphores.

All TAs access the shared rubric and the `completed_questions[]` array without
protection. Different TAs can modify the same rubric line or choose the same
question at the same time.

Because of that:

- **Mutual exclusion** is not enforced.  
- **Progress** and **bounded waiting** are not guaranteed and depend on the OS
  scheduler and race conditions.

This version is intentionally unsafe.

### Part 2b (with semaphores)

`Part2b_101310113_101256959.c` adds two POSIX semaphores in shared memory:

- `rubric_mutex` – protects reading/writing the rubric.  
- `question_mutex` – protects access to `completed_questions[]` when a TA picks
  the next question.

Each critical section is wrapped with:

```c
// lock the shared data
sem_wait(&shared->rubric_mutex);

// use / change the shared data here

// unlock the shared data
sem_post(&shared->rubric_mutex);

