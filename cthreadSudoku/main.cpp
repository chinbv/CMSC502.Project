
#include "sudoku.cpp"
#include "sudokuPermute.cpp"
#include <thread>

using namespace std;

const int num_threads = 9;

int grids[num_threads][9][9];
vector<int> thread_queue;


void call_from_thread(int thread_num) {
    if (SolveSudoku(grids[thread_num]) == true) {
        printf("Num: %d\n", thread_num);
        printGrid(grids[thread_num]);
    }
    else {
        printf("No solution exists\n");
        //printGrid(grids[thread_num]);
    }
    return;
}


int main()
{
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
    for(int i = 0; i < num_threads; i++) {
        thread_queue.push_back(i);
        for(int j = 0; j < 9; j ++) {
            std::copy(std::begin(array[j]), std::end(array[j]), std::begin(grids[i][j]));
        }
    }

    // Init permutation number (permutes the first few values of the sudoku
    int first_values[3] = {1, 1, 1};

    // Find the first couple of zero indexes
    int zeroIndex[3][2];
    int count = 0;
    for(int i = 0; i < 81; i++) {
        for(int j = 0; j < 81; j++) {
            if(array[i][j] == 0) {
                zeroIndex[count][0] = i;
                zeroindex[count][1] = j;
            }
            if(count >= 3) break;
        }
        if(count >= 3) break;
    }

    // Pick a thread from queue, run it on next iteration
    bool safe;
    thread t[num_threads];
    do {
        safe = true;
        // Check if the current permutation is safe
        for(int i = 0; i < 3; i++) {
            if(!isSafe(array, zeroIndex[i][0], zeroIndex[i][1], first_values[i])) {
                safe = false;
                break;
            }
        }
        if(safe) {

            thread_queue.

            // Assign the current permutation
            for(int i = 0; i < 3; i++) {
                grids[i] [zeroIndex[i][0]] [zeroIndex[i][1]] = first_values[i];
            }
        }

        sudokuPermuteNext(first_values);

    } while(sudokuPermuteNext(first_values));


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