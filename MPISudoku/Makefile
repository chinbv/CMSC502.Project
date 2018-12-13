program.exe : main.cpp sudoku.cpp
	mpic++ main.cpp -o program.exe -std=c++11 -pthread -g

run:
	mpirun -np 9 ./program.exe
