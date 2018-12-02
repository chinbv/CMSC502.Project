

int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int rank, nthreads;
    MPI_Comm_size(MPI_COMM_WORLD, &nthreads);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    printf("my rank is %d\n", rank);

    return 0;

}
