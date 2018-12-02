
#include "sudoku.cpp"
#include "sudokuPermute.cpp"
#include <thread>
#include <vector>
#include <mutex>
#include <semaphore.h>

using namespace std;

// TODO take number of threads as program input
const int num_threads = 9;
mutex thread_mutexes[num_threads];
thread threads[num_threads];

int grids[num_threads][9][9];
vector<int> thread_queue;
sem_t thread_queue_semaphore;
mutex thread_queue_mutex;


// call this function from each thread, using different starting sudoku values
void call_from_thread(int thread_num) {

    while(true) {

        // wait until main thread gives this one permission to continue
        thread_mutexes[thread_num].lock();

        // attempt a solve
        if (SolveSudoku(grids[thread_num])) {
            printf("Num: %d\n", thread_num);
            printGrid(grids[thread_num]);
        } else {
            //printf("No solution exists\n");
            //printGrid(grids[thread_num]);
        }

        // return the thread to the thread queue
        thread_queue_mutex.lock();
        thread_queue.push_back(thread_num);
        thread_queue_mutex.unlock();
        sem_post(&thread_queue_semaphore);

    }

    return;
}


int main()
{
    // TODO take this input from file
    int array[9][9] = {{3, 0, 6, 5, 0, 8, 4, 0, 0},
                       {5, 2, 0, 0, 0, 0, 0, 0, 0},
                       {0, 8, 7, 0, 0, 0, 0, 3, 1},
                       {0, 0, 3, 0, 1, 0, 0, 8, 0},
                       {9, 0, 0, 8, 6, 3, 0, 0, 5},
                       {0, 5, 0, 0, 9, 0, 6, 0, 0},
                       {1, 3, 0, 0, 0, 0, 2, 5, 0},
                       {0, 0, 0, 0, 0, 0, 0, 7, 4},
                       {0, 0, 5, 2, 0, 6, 3, 0, 0}};

    // Init grids and thread queue
    // Each thread needs a full copy of the initial sudoku board
    for(int i = 0; i < num_threads; i++) {
        thread_mutexes[i].lock();
        thread_queue.push_back(i);
        threads[i] = thread(call_from_thread, i);
        for(int j = 0; j < 9; j ++) {
            std::copy(std::begin(array[j]), std::end(array[j]), std::begin(grids[i][j]));
        }
    }

    // Init permutation number (permutes the first few values of the sudoku puzzle)
    int num_values_to_permute = 3;
    int first_values[num_values_to_permute] = {1, 1, 1};
    int zeroIndex[3][2];


    // Find the first couple of zero indexes
    int permuteCount = 0;
    for(int i = 0; i < 9; i++) {
        for(int j = 0; j < 9; j++) {
            if(array[i][j] == 0) {
                zeroIndex[permuteCount][0] = i;
                zeroIndex[permuteCount][1] = j;
                permuteCount++;
            }
            if(permuteCount >= num_values_to_permute) break;
        }
        if(permuteCount >= num_values_to_permute) break;
    }

    // init semaphore for thread_queue
    sem_init(&thread_queue_semaphore, 0, thread_queue.size());

    // From here on, use "permuteCount"
    // Pick a thread from queue, run it on next iteration
    bool safe;
    thread t[num_threads];
    do {
//        for(int i = 0; i < permuteCount; i++) {
//            printf("%d ", first_values[i]);
//        }
//        printf("\n");

        safe = true;
        // Check if the current permutation is safe
        for(int i = 0; i < permuteCount; i++) {
            if(!isSafe(array, zeroIndex[i][0], zeroIndex[i][1], first_values[i])) {
                safe = false;
                break;
            }
        }
        if(safe) {

            // wait for thread to be available, then securely access the thread queue
            sem_wait(&thread_queue_semaphore);
            thread_queue_mutex.lock();
            int availableThread = thread_queue.back();
            thread_queue.pop_back();
            thread_queue_mutex.unlock();

            // Assign the current permutation
            for(int i = 0; i < 3; i++) {
                grids[availableThread] [zeroIndex[i][0]] [zeroIndex[i][1]] = first_values[i];
            }

            // Wake up thread
            thread_mutexes[availableThread].unlock();
        }

    } while(sudokuPermuteNext(first_values, permuteCount));


    for(int i = 0; i < 9; i++) {
        if(isSafe(grids[i], 0, 1, i+1)) {
            grids[i][0][1] = i+1;
            t[i] = thread(call_from_thread, i);
        }
    }

    for(int i = 0; i < num_threads; i++){
        if(t[i].joinable()) t[i].join();
    }

    return 0;
}