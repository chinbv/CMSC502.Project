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
#include <chrono>  // for high_resolution_clock'
#include "threadSudoku.cpp"


/*
 * Bugs:
 *
 * for some reason, thread management is just receiving 2's from processes on tag 7
 */


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

int sudokuArray2[9][9] = {{3, 0, 6, 5, 0, 8, 4, 0, 0},
                         {5, 2, 0, 0, 0, 0, 0, 0, 0},
                         {0, 8, 7, 0, 0, 0, 0, 3, 1},
                         {0, 0, 3, 0, 1, 0, 0, 8, 0},
                         {9, 0, 0, 8, 6, 3, 0, 0, 5},
                         {0, 5, 0, 0, 9, 0, 6, 0, 0},
                         {1, 3, 0, 0, 0, 0, 2, 5, 0},
                         {0, 0, 0, 0, 0, 0, 0, 7, 4},
                         {0, 0, 5, 2, 0, 6, 3, 0, 0}};


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

        //printf("Adding rank %d to queue\n", status.MPI_SOURCE);

        processQueueMutex.lock();
        processQueue.push_back(status.MPI_SOURCE);
        sem_post(&processQueueSemaphore);

        printf("queue: ");
        for(int i = 0; i < processQueue.size(); i++) {
            printf("%d, ", processQueue[i]);
        }
        printf("\n");

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

    // If there is no unassigned location, we are done
    if(!FindUnassignedLocation(grid, row, col)) {
        cout << "something bad probably just happened" << endl;
        return true; // success!
    }

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

    int zeroIndex1D[permuteCount*2];
    for(int i = 0; i < permuteCount; i++) {
        zeroIndex1D[i*2] = zeroIndex[i][0];
        zeroIndex1D[i*2+1] = zeroIndex[i][1];
    }

    // Send permute count to all slaves
    for(int i = 1; i < nthreads; i++) {
        MPI_Send(&permuteCount, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
    // Send zeroIndexes to all slaves
    for(int i = 1; i < nthreads; i++) {
        MPI_Send(zeroIndex1D, permuteCount*2, MPI_INT, i, 5, MPI_COMM_WORLD);
    }

    // Pick a thread from queue, run it on next iteration
    bool safe;
    permuteSudoku(sudokuArray, first_values, permuteCount, zeroIndex, 0);

    int numbers[permuteCount];
    numbers[0] = -1;

    for(int i = 1; i < nthreads; i++) {
        MPI_Send(numbers, permuteCount, MPI_INT, i, 17, MPI_COMM_WORLD);
    }

    threadQueueManagement.join();

}
//
///*
// * variables used in manipulation of threads for slave processes
// */
//const int num_threads = 10;
//mutex thread_mutexes[num_threads];
//std::thread the_threads[num_threads];
//
//int grids[num_threads][9][9];
//vector<int> thread_queue;
//sem_t thread_queue_semaphore;
//mutex thread_queue_mutex;
//bool thread_received_permutation[num_threads];
//
//bool threadsAreFinished;

//// Call this function from each thread, using different starting sudoku values
//void call_from_thread(int thread_num, int rank) {
//
//    while(true) {
//
//        // Wait until main thread gives this one permission to continue
//        if(rank == 3) printf("stuck? %d\n", thread_num);
//        thread_mutexes[thread_num].lock();
//        if(rank == 3) printf("not %d\n", thread_num);
//
//        //printf("I am a thread and i just locked\n");
//
//        // If solution has been found by another thread, return
//        // This has to be done immediately after the unlock, since the main will unlock all threads when solution is found
//        if(thread_received_permutation[thread_num]) {
//            // Attempt a solve, return if successful
//            if (SolveSudoku(grids[thread_num])) {
//                //printGrid(grids[thread_num]);
//            }
//
//            // return the thread to the thread queue
//            thread_queue_mutex.lock();
//            thread_queue.push_back(thread_num);
//            sem_post(&thread_queue_semaphore);
//            thread_queue_mutex.unlock();
//            thread_received_permutation[thread_num] = false;
//        } else {
//            return;
//        }
//    }
//}
//
//bool permuteSudokuForThreads(int grid[N][N], int first_values[], int permuteCount, int zeroIndex[][2], int someCount) {
//
//    int row = 0, col = 0;
//
//    // If there is no unassigned location, we are done
//    if(!FindUnassignedLocation(grid, row, col)) {
//        cout << "something bad probably just happened" << endl;
//        return true; // success!
//    }
//
//    // consider digits 1 to 9
//    for (int num = 1; num <= 9; num++)
//    {
//        // if looks promising
//        if (isSafe(grid, row, col, num))
//        {
//            // make tentative assignment
//            grid[row][col] = num;
//
//            // assign the corresponding first_value
//            first_values[someCount] = num;
//
//            // if we are at the last zeroIndex, go ahead and send, and return
//            if (row == zeroIndex[permuteCount-1][0] && col == zeroIndex[permuteCount-1][1]) {
//                // wait for process to be available, then securely access the process queue
//                sem_wait(&thread_queue_semaphore);
//                thread_queue_mutex.lock();
//
//                int availableThread = thread_queue.back();
//                thread_queue.pop_back();
//                thread_queue_mutex.unlock();
//
//                // Assign the current permutation
//                for(int i = 0; i < permuteCount; i++) {
//                    grids[availableThread] [zeroIndex[i][0]] [zeroIndex[i][1]] = first_values[i];
//                }
//
//                thread_received_permutation[availableThread] = true;
//                thread_mutexes[availableThread].unlock();
//            } else {
//                permuteSudokuForThreads(grid, std::ref(first_values), permuteCount, zeroIndex, someCount+1);
//            }
//        }
//        grid[row][col] = UNASSIGNED;
//
//    }
//    return false; // this triggers backtracking
//
//}

void runSlave(int nthreads, int rank) {

    // get permuteCount
    int permuteCount;
    MPI_Recv(&permuteCount, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int zeroIndex[permuteCount*2];
    MPI_Recv(zeroIndex, permuteCount*2, MPI_INT, 0, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int first_values[permuteCount];
    while(true) {
        MPI_Recv(first_values, permuteCount, MPI_INT, 0, 17, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (first_values[0] == -1) {
            int number = -1;
            MPI_Send(&number, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
            break;
        }

        // Fill in sudoku with first_values
        for (int i = 0; i < permuteCount; i++) {
            sudokuArray[zeroIndex[i * 2]][zeroIndex[i * 2 + 1]] = first_values[i];
        }

        printf("before %d\n", rank);

        something(sudokuArray);





//        for(int i = 0; i < num_threads; i++) {
//            thread_mutexes[i].unlock();
//            thread_received_permutation[i] = false;
//        }
//
//        thread_queue.clear();
//        thread_queue_mutex.unlock();
//
//        bool foundSolution;
//
//        // init semaphore for thread_queue
//        sem_init(&thread_queue_semaphore, 0, 0);
//
//        foundSolution = false;
//
//        // Init grids and thread queue
//        // Each thread needs a full copy of the initial sudoku board
//        for(int i = 0; i < num_threads; i++) {
//            thread_mutexes[i].lock();
//            thread_queue.push_back(i);
//            sem_post(&thread_queue_semaphore);
//            //printf("init thread rank %d\n", rank);
//            the_threads[i] = thread(call_from_thread, i, rank);
//            for(int j = 0; j < 9; j ++) {
//                std::copy(std::begin(sudokuArray[j]), std::end(sudokuArray[j]), std::begin(grids[i][j]));
//            }
//        }
//
//        // Init permutation number (permutes the first few values of the sudoku puzzle)
//        int num_values_to_permute = ceil(sqrt(num_threads));
//        int first_values2[num_values_to_permute];
//        for(int i = 0; i < num_values_to_permute; i++) {
//            first_values2[i] = 1;
//        }
//        int zeroIndex2[num_values_to_permute][2];
//
//
//        // Find the first couple of zero indexes
//        int permuteCount2 = 0;
//        for(int i = 0; i < 9; i++) {
//            for(int j = 0; j < 9; j++) {
//                if(sudokuArray[i][j] == 0) {
//                    zeroIndex2[permuteCount2][0] = i;
//                    zeroIndex2[permuteCount2][1] = j;
//                    permuteCount2++;
//                }
//                if(permuteCount2 >= num_values_to_permute) break;
//            }
//            if(permuteCount2 >= num_values_to_permute) break;
//        }
//
//        /*
//         * THIS IS THE BIG ONE
//         */
//        //printf("Before permute %d\n", rank);
//
//        permuteSudokuForThreads(sudokuArray, first_values2, permuteCount2, zeroIndex2, 0);
//
//        //printf("After permute %d\n", rank);
//
//        // We don't want to lock here if we haven't found the solution
//        // However, we do want to make sure all of the threads have finished, right?
//        // when permute is done, some threads may still be running
//        // We need to signal threads that are still running, and threads that have stopped that they are good to continue
//
//        // All processes are finishing permuting
//        // all processes are unlocking the threads
//        // NONE are joining threads, no threads are joining
//
//        // Need to make sure all threads have actually processed their last permutation, or AT LEAST have started
//
//        foundSolution = true;
//        threadsAreFinished = true;
//        for(int i = 0; i < num_threads; i++) {
//            thread_mutexes[i].unlock();
//        }
//
//
////        printf("unlocked threads %d\n", rank);
//
//
//        printf("BEGIN %d\n", rank);
//
//        for(int i = 0; i < num_threads; i++){
//            if(the_threads[i].joinable()) {
//                the_threads[i].join();
//            }
////            printf("JOINED A THREAD %d %d\n", i, rank);
//        }

        printf("END %d\n", rank);

        //printf("joined threads %d\n", rank);


        MPI_Send(&rank, 1, MPI_INT, 0, 7, MPI_COMM_WORLD);
    }

    printf("slave finished %d\n", rank);
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
