#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <mpi.h>

//this function is used for putting initial values to the array.
void set_val(int a[][9], int x, int y, int val){
	a[x][y] = val;
}
//constraint propagation method is used for this sudoku solver.
void const_prop(int array[][9],int x,int y, int inf[9][10], int index){
	int i, j;
	//checking whole row for existing values
	//index represents element order in grid is being worked.
	for(i = 0; i < 9; i++){
		if(array[x][i] != 0 && inf[index][array[x][i]] != 0){
			inf[index][array[x][i]] = 0;//zero means this value is not available for this index. inf[0][5] = 0 means first element
			inf[index][0]--;//counter decremented. When it is one, there is only one element can be put this index.
		}
	}
	//checking whole column for existing values
	for(i = 0; i < 9; i++){
		if(array[i][y] != 0 && inf[index][array[i][y]] != 0){
			inf[index][array[i][y]] = 0;
			inf[index][0]--;
		}
	}
	// Grid boundaries calculation
	// b and c values are unnecessary
	int a,b;
	int c,d;
	if(x < 3){
		a = 0;
		b = 3;
	}
	else if ( x < 6){
		a = 3;
		b = 6;
	}
	else{
		a = 6;
		b = 9;
	}

	if(y < 3){
		c = 0;
		d = 3;
	}
	else if ( y < 6){
		c = 3;
		d = 6;
	}
	else{
		c = 6;
		d = 9;
	}
	//checking whole grid for existing values
	for(i = a; i < b; i++){
		for(j = c; j < d; j++){
			if(array[i][j] != 0 && inf[index][array[i][j]] != 0){
				inf[index][array[i][j]] = 0;
				inf[index][0]--;
			}
		}
	}
}

//this function checks if there is any value has been found in this grid.
void is_it_found(int inf[9][10], int a[3], int rank){
	int i,j;//counters for for loops
	int row,col;//variables that are used for calculating row and column indexes.
	for(i = 0; i < 9; i++){
		if(inf[i][0] == 1){//this means there is only one value that can be put to the cell.
			if ((rank-1) < 3)
				row = 0;
			else if ((rank-1) < 6)
				row = 3;
			else
				row = 6;

			if((rank-1) % 3 == 0)
				col = 0;
			else if((rank-1) % 3 == 1)
				col = 3;
			else
				col = 6;
			//grid start indexes calculated above. Afterwards, calculating exact index with the help of element order.
			row = row + floor(i/3);
			col = col + (i%3);
			//array is being written
			a[0] = row;
			a[1] = col;
			//finding which number is going to be put here.
			for(j = 1; j < 10; j++)
				if(inf[i][j] == 1){
					a[2] = j;
					return;//This function is finding one and only one value each time.
				}
		}
	}
}

//main start
int main(int argc, char** argv) {
    
	int my_rank;   /* My process rank           */
	int p;         /* The number of processes   */
	int	source;    /* Process sending integral  */
	int	dest = 0;
	int	tag = 0;
	int sudoku[9][9];//main sudoku array for master process.
	int sudoku_copy[9][9];//slave processes use this array to keep their values.
	int info[9][10];//information array about grid.
	int i, j, a, b, k,m,c,d;//numbers used for different purposes, mostly counters.
	int displacements[9][9];//Used for indexed data type.
	int block_lengths[9][9];//Used for indexed data type.
    
	MPI_Datatype indexed_mpi_t[9];//Indexed data type which is being used for sending different data to different processes.
    
	int found_value[3];//array for representing the value has been found. First 2 elements represent x and y index accordingly.Third one is for the value.
	int number_of_found_elements;//counter for stopping while loops.
    
	//initializing information array
	//This array represents grids' elements respectively. Each row represents each element.
	for(i = 0; i < 9; i++){
		for(j = 0; j < 10; j++){
			if( j == 0)
				info[i][j] = 9; //initally 9 numbers can be put this place.
			else
				info[i][j] = 1; //initally all numbers all available for this place.
			}
	}
    
	MPI_Status  status;
	/* Let the system do what it needs to start up MPI */
	MPI_Init(&argc, &argv);
	/* Get my process rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	/* Find out how many processes are being used */
	MPI_Comm_size(MPI_COMM_WORLD, &p);
	//9 grids each numbered respectively 0 to 8.
	for(i=0; i<9; i++){
		//initializing a variable. This variable sets the block lenghts for different grids.
		if (i < 3)
			a = 0;
		else if (i < 6)
			a = 3;
		else
			a = 6;
		//setting block lenghts.
		for(j = 0; j < 9; j++)
			block_lengths[i][j] = 3;
		for(j = a; j < a+3; j++)
			block_lengths[i][j] = 9;
        
        /* Crappy print out of the block_lengths matrix
        if(my_rank - 1 ==  0)
        for(int counterOne = 0; counterOne < 9; ++counterOne) {
            for(int counterTwo = 0; counterTwo < 9; ++counterTwo) {
                printf("block_lengths[%d][%d]\n", block_lengths[counterOne][counterTwo], block_lengths[counterOne][counterTwo]);
            }
        }
        printf("\n");
         */
        
		//initalizing b variable. This variable sets the displacements for different grids.
		if(i % 3 == 0)
			b = 0;
		else if(i % 3 == 1)
			b = 3;
		else
			b = 6;
		//setting displacements.
		for(j = 0; j < 9; j++)
			displacements[i][j] = 9*j + b;
		if(a == 0){
			for (j = 0; j < 3; j++ )
				displacements[i][j] = 9*j;
		} else if (a == 3){
			for (j = 3; j < 6; j++ )
				displacements[i][j] = 9*j;
		}else if (a == 6){
			for (j = 6; j < 9; j++ )
				displacements[i][j] = 9*j;
		}
	}
	//indexed method for sending array to the slaves.
	for(i = 0; i < 9; i++){
		MPI_Type_indexed(9,block_lengths[i],displacements[i], MPI_INT, &indexed_mpi_t[i]);
		MPI_Type_commit(&indexed_mpi_t[i]);
	}
	//MASTER PROCESS
	//Dealing with whole sudoku. exchanging information with slaves. When new value is found, puts it to sudoku and sending new sudoku
	//to the slaves.
	if(my_rank == 0){
		for (i = 0; i < 9; i++)
			for (j = 0; j < 9; j++)
				sudoku[i][j] = 0;
		//Giving initial SUDOKU values.
		sudoku[0][3] = 9;
		sudoku[0][6] = 7;
		sudoku[0][7] = 2;
		sudoku[0][8] = 8;
		sudoku[1][0] = 2;
		sudoku[1][1] = 7;
		sudoku[1][2] = 8;
		sudoku[1][5] = 3;
		sudoku[1][7] = 1;
		sudoku[2][1] = 9;
		sudoku[2][6] = 6;
		sudoku[2][7] = 4;
		sudoku[3][1] = 5;
		sudoku[3][4] = 6;
		sudoku[3][6] = 2;
		sudoku[4][2] = 6;
		sudoku[4][6] = 3;
		sudoku[5][1] = 1;
		sudoku[5][4] = 5;
		sudoku[6][0] = 1;
		sudoku[6][3] = 7;
		sudoku[6][5] = 6;
		sudoku[6][7] = 3;
		sudoku[6][8] = 4;
		sudoku[7][3] = 5;
		sudoku[7][5] = 4;
		sudoku[8][0] = 7;
		sudoku[8][2] = 9;
		sudoku[8][3] = 1;
		sudoku[8][6] = 8;
		sudoku[8][8] = 5;
		//initally 31 elements are given.
		number_of_found_elements = 31;
		//sending initial information to the slaves.
		for(dest = 1; dest < p; dest++){
			tag = 0;//tag 0 sends 9 grids, with their rowns and columns to the slaves.
			MPI_Send(sudoku, 1, indexed_mpi_t[dest-1], dest, tag,MPI_COMM_WORLD);
			tag = 2;//tag 2 sends elements have been found to the slaves.
			MPI_Send(&number_of_found_elements,1,MPI_INT,dest,tag,MPI_COMM_WORLD);
		}
		//loop for finding all sudoku elements.
		while(number_of_found_elements < 81){
			//Printing current sudoku condition.
			printf("#######CURRENT SUDOKU####### \n");
			for(i = 0; i < 9; i++){
				for(j = 0; j < 9; j++)
					printf("%3.1d",sudoku[i][j]);
					printf("\n");
			}
			printf("NUMBER OF ELEMENTS FOUND %d \n", number_of_found_elements);
			printf("\n");
			printf("\n");
			//recieving new values from slaves.
			for(dest = 1; dest < p; dest++){
				tag = 1;
				MPI_Recv(&(found_value[0]), 3, MPI_INT, dest, tag,MPI_COMM_WORLD, &status);
				if(found_value[0] != -1){
						sudoku[found_value[0]][found_value[1]] = found_value[2];
						printf("RANK : %d sent value. X: %d Y: %d Value: %d\n",dest, found_value[0], found_value[1], found_value[2]);
						number_of_found_elements++;
					}
			}
			//sending number of elements has been found to the slaves.
			for(dest = 1; dest < p; dest++){
				tag = 2;
				MPI_Send(&number_of_found_elements,1,MPI_INT,dest,tag,MPI_COMM_WORLD);
			}
			//sending new values to the slaves.
			for(dest = 1; dest < p; dest++){
				tag = 0;
				MPI_Send(sudoku, 1, indexed_mpi_t[dest-1], dest, tag,MPI_COMM_WORLD);
			}
		}
	}
	else{
		do{
			//first gets data of how many elements have been found.
			tag = 2;
			MPI_Recv(&number_of_found_elements, 1, MPI_INT, 0, tag,MPI_COMM_WORLD, &status);
			//sets found_value array to not found.
			found_value[0] = -1; //row index
			found_value[1] = -1; //col index
			found_value[2] = -1; //value

			//gets the data of needed rows and columns.
			tag = 0;
			MPI_Recv(sudoku_copy, 1, indexed_mpi_t[my_rank-1], 0, tag,MPI_COMM_WORLD, &status);
			//a and b are starting index values of grids' first elements.
			if (my_rank-1 < 3)
				a = 0;
			else if (my_rank-1 < 6)
				a = 3;
			else
				a = 6;
			if((my_rank-1) % 3 == 0)
				b = 0;
			else if((my_rank-1) % 3 == 1)
				b = 3;
			else
				b = 6;
			//counter for selected grid's elements.
			m = 0;
			for(i = a; i < a+3; i++){
				for(j = b; j < b+3; j++){
					if(sudoku_copy[i][j] == 0){
						//const_prop function is being called.
						const_prop(sudoku_copy,i,j,info,m);
					}else
						info[m][0] = -1;//not needed just to be sure.
					m++;
				}
			}
			//is_it_found function is being called.
			is_it_found(info, found_value, my_rank);
			//sending found_value. Even if nothing has been found, sends not-found information to the master process.
			tag = 1;
			MPI_Send(&(found_value[0]),3,MPI_INT,0,tag,MPI_COMM_WORLD);
		}while(number_of_found_elements != 81);//looping until all elements have been found.
	}
	/* Shut down MPI */
	MPI_Finalize();
} /*  main  */
