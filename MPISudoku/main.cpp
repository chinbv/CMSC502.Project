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
#include <chrono>  // for high_resolution_clock


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

void printArray(int array[], int size) {
    for(int i = 0; i < size; i++) {
        cout << array[i] << ", ";
    }
    cout << endl;
}

bool permuteSudoku(int grid[N][N], int first_values[], int permuteCount, int zeroIndex[][2], int someCount) {

    int row = 0, col = 0;

    //cout << "someCount " << someCount << endl;

    // If there is no unassigned location, we are done
    if(!FindUnassignedLocation(grid, row, col)) {
        cout << "something bad probably just happened" << endl;
        return true; // success!
    }

    //cout << "row " << row << " col " << col << endl;

    // consider digits 1 to 9
    for (int num = 1; num <= 9; num++)
    {
        // if looks promising
        if (isSafe(grid, row, col, num))
        {
            // make tentative assignment
            grid[row][col] = num;

            // assign the corresponding first_value
            first_values[someCount] = num;

            // if we are at the last zeroIndex, go ahead and send, and return
            if (row == zeroIndex[permuteCount-1][0] && col == zeroIndex[permuteCount-1][1]) {
                // wait for process to be available, then securely access the process queue
                sem_wait(&processQueueSemaphore);
                processQueueMutex.lock();
                int availableProcess = processQueue.back();
                processQueue.pop_back();
                processQueueMutex.unlock();
                //cout << permuteCount << endl;

                cout << " send to " << availableProcess << " ";
                printArray(first_values, permuteCount);

                MPI_Send(first_values, permuteCount, MPI_INT, availableProcess, 17, MPI_COMM_WORLD);

            } else {
                permuteSudoku(grid, std::ref(first_values), permuteCount, zeroIndex, someCount+1);
            }
        }
        grid[row][col] = UNASSIGNED;

    }
    return false; // this triggers backtracking

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
    int num_values_to_permute = ceil(5*sqrt(nthreads));
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

    cout << "calculated permute count " << permuteCount << endl;

    int zeroIndex1D[permuteCount*2];
    for(int i = 0; i < permuteCount; i++) {
        zeroIndex1D[i*2] = zeroIndex[i][0];
        zeroIndex1D[i*2+1] = zeroIndex[i][1];
    }

    cout << "created 1D zeroIndex " << endl;

    // Send permute count to all slaves
    for(int i = 1; i < nthreads; i++) {
        MPI_Send(&permuteCount, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
    // Send zeroIndexes to all slaves
    for(int i = 1; i < nthreads; i++) {
        MPI_Send(&zeroIndex1D, permuteCount*2, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    cout << "did sends " << endl;

    // Pick a thread from queue, run it on next iteration
    bool safe;
    permuteSudoku(sudokuArray, first_values, permuteCount, zeroIndex, 0);

//    do {
//        permuteSudoku(sudokuArray, first_values, permuteCount, zeroIndex, 0);
//
////        safe = true;
////
////        // Add all permuted values into the sudoku
////        for(int i = 0; i < permuteCount; i++) {
////            sudokuArray[zeroIndex[i][0]][zeroIndex[i][1]] = first_values[i];
////        }
////
////        // Check if the current permutation is safe
////        for(int i = 0; i < permuteCount; i++) {
////            sudokuArray[zeroIndex[i][0]][zeroIndex[i][1]] = 0;
////            if(!isSafe(sudokuArray, zeroIndex[i][0], zeroIndex[i][1], first_values[i])) {
////                safe = false;
////                break;
////            }
////            sudokuArray[zeroIndex[i][0]][zeroIndex[i][1]] = first_values[i];
////        }
////        if(safe) {
////
////            for(int i = 0; i < permuteCount; i++) {
////                printf("%d, ", first_values[i]);
////            }
////            printf("\n");
////
////            // wait for process to be available, then securely access the process queue
////            sem_wait(&processQueueSemaphore);
////            processQueueMutex.lock();
////            int availableProcess = processQueue.back();
////            processQueue.pop_back();
////            processQueueMutex.unlock();
////
////            // Assign the current permutation
////            // TODO do some permutation sending here
////            MPI_Send(&first_values, permuteCount, MPI_INT, availableProcess, 0, MPI_COMM_WORLD);
////
////        }
//
//    } while(sudokuPermuteNext(first_values, permuteCount));

    //printf("Hi from master\n");

    int numbers[permuteCount];
    numbers[0] = -1;

    for(int i = 1; i < nthreads; i++) {
        MPI_Send(numbers, permuteCount, MPI_INT, i, 17, MPI_COMM_WORLD);
    }

    //printf("I want to join\n");

    threadQueueManagement.join();

}

void runSlave(int nthreads, int rank) {

    // get permuteCount
    int permuteCount;
    MPI_Recv(&permuteCount, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int zeroIndex[permuteCount];
    MPI_Recv(&zeroIndex, permuteCount*2, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


    int first_values[permuteCount];
    while(true) {
        //cout << "something" << endl;
        MPI_Recv(&first_values, permuteCount, MPI_INT, 0, 17, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(first_values[0] == -1) {
            int number = -1;
            MPI_Send(&number, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
            break;
        }

        // Fill in sudoku with first_values
        for(int i = 0; i < permuteCount; i++) {
            sudokuArray[zeroIndex[i*2]][zeroIndex[i*2+1]] = first_values[i];
        }

        // Attempt a solve, return if successful
        if (SolveSudoku(sudokuArray)) {
            printGrid(sudokuArray);
        }

        MPI_Send(&rank, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
    }

    //printf("Slave done, rank %d\n", rank);

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

    MPI_Barrier( MPI_COMM_WORLD );

    // Record start time
    auto start = std::chrono::high_resolution_clock::now();

    if(rank == 0) {
        runMaster(nthreads, rank);
    } else {
        runSlave(nthreads, rank);
    }

    //printf("DONE%d\n", rank);

    MPI_Barrier( MPI_COMM_WORLD );

    // Record end time
    if(rank == 0) {
        auto finish = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = finish - start;
        std::cout << "Elapsed time: " << elapsed.count() << " s\n";
    }

    MPI_Finalize();

    return 0;

}
