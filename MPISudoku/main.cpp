#include "../cthreadSudoku/sudoku.cpp"
#include "../cthreadSudoku/sudokuPermute.cpp"
#include <thread>
#include <vector>
#include <mutex>
#include <semaphore.h>
#include <cmath>
#include <mpi.h>
#include <unistd.h>
#include <iostream>

using namespace std;

// TODO take this input from file
int sudokuArray[9][9] = {{3, 0, 6, 5, 0, 8, 4, 0, 0},
                         {5, 2, 0, 0, 0, 0, 0, 0, 0},
                         {0, 8, 7, 0, 0, 0, 0, 3, 1},
                         {0, 0, 3, 0, 1, 0, 0, 8, 0},
                         {9, 0, 0, 8, 6, 3, 0, 0, 5},
                         {0, 5, 0, 0, 9, 0, 6, 0, 0},
                         {1, 3, 0, 0, 0, 0, 2, 5, 0},
                         {0, 0, 0, 0, 0, 0, 0, 7, 4},
                         {0, 0, 5, 2, 0, 6, 3, 0, 0}};

//void manageQueues(vector<int> processQueue,
//        sem_t processQueueSemaphore,
//        mutex processQueueMutex) {
//
//    printf("Hi from queue manager\n");
//
//}
vector<int> processQueue;
mutex processQueueMutex;
sem_t processQueueSemaphore;

void manageQueues() {

//    int value;
//
//    processQueueMutex.lock();
//    sem_getvalue(&processQueueSemaphore, &value);
//    processQueueMutex.unlock();

    int number;
    MPI_Status status;
    while(true) {
        MPI_Recv(&number, 1, MPI_INT, MPI_ANY_SOURCE, 7, MPI_COMM_WORLD, &status);
        if(number == -1) { break; }

        processQueueMutex.lock();
        processQueue.push_back(number);
        sem_post(&processQueueSemaphore);
        processQueueMutex.unlock();
    }

    return;

}

void runMaster(int nthreads, int rank) {

    /*
     * Init process queue
     */
    processQueueMutex.lock();
    sem_init(&processQueueSemaphore, 0, 0);
    for(int i = 1; i < nthreads; i++) {

        processQueue.push_back(i);
        sem_post(&processQueueSemaphore);

    }
    processQueueMutex.unlock();

    /*
     * Start queue management thread
     */
    std::thread threadQueueManagement(manageQueues);



    // Init permutation number (permutes the first few values of the sudoku puzzle)
    int num_values_to_permute = ceil(sqrt(nthreads));
    int first_values[num_values_to_permute];
    for(int i = 0; i < num_values_to_permute; i++) {
        first_values[i] = 1;
    }
    int zeroIndex[num_values_to_permute][2];

    // Find the first couple of zero indexes
    int permuteCount = 0;
    for(int i = 0; i < 9; i++) {
        for(int j = 0; j < 9; j++) {
            if(sudokuArray[i][j] == 0) {
                zeroIndex[permuteCount][0] = i;
                zeroIndex[permuteCount][1] = j;
                permuteCount++;
            }
            if(permuteCount >= num_values_to_permute) break;
        }
        if(permuteCount >= num_values_to_permute) break;
    }

    // Pick a thread from queue, run it on next iteration
    bool safe;
    do {
        safe = true;

        // Check if the current permutation is safe
        for(int i = 0; i < permuteCount; i++) {
            if(!isSafe(sudokuArray, zeroIndex[i][0], zeroIndex[i][1], first_values[i])) {
                safe = false;
                break;
            }
        }
        if(safe) {

            // wait for process to be available, then securely access the process queue
            sem_wait(&processQueueSemaphore);
            processQueueMutex.lock();
            int availableProcess = processQueue.back();
            processQueue.pop_back();
            processQueueMutex.unlock();

            // Assign the current permutation
            // TODO do some permutation sending here
            int number = 0;
            MPI_Send(&number, 1, MPI_INT, availableProcess, 0, MPI_COMM_WORLD);

        }

    } while(sudokuPermuteNext(first_values, permuteCount));

    printf("Hi from master\n");

    int number = -1;
    for(int i = 1; i < nthreads; i++) {
        MPI_Send(&number, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    printf("I want to join\n");

    threadQueueManagement.join();
}

void runSlave(int nthreads, int rank) {

    int number;
    while(true) {
        cout << "something" << endl;
        MPI_Recv(&number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(number == -1) {
            number = -1;
            MPI_Send(&number, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
            break;
        }
        MPI_Send(&rank, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
    }

    printf("Slave done, rank %d\n", rank);


}

int main(int argc, char** argv) {
    int provided;

    MPI_Init(&argc, &argv);

    int rank, nthreads;
    MPI_Comm_size(MPI_COMM_WORLD, &nthreads);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*
     * 1) seperate into main process and sub-processes
     * 2) start sending permutations to processes
     * 2.a) main process can have 2 threads, one which manages queues, and one which sends permutations
     * 3) sub processes should process permutations, if find
     */

    printf("my rank is %d\n", rank);

    if(rank == 0) {
        runMaster(nthreads, rank);
    } else {
        runSlave(nthreads, rank);
    }

    printf("DONE%d\n", rank);

    MPI_Finalize();

    return 0;

}
