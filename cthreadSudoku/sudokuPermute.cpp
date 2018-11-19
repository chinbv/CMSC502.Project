
bool sudokuPermuteNext(int array[], int size) {

    for(int i = size - 1; i >= 0; i--) {
        array[i] += 1;
        if(array[i] <= 9) {
            return true;
        }
        else {
            array[i] = 1;
        }
    }

    return false;
}

