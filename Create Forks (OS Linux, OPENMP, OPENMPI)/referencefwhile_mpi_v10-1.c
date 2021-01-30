/**
 * The program is proposed to find out whether the specified number is prime or not.
 * The source number is provided as first command-line parameter.
 * The result is printed out on stdout.
 *
 * To compile the program say:
 *    mpicc -Wall referencefwhile_mpi_v10-1.c -o referencefwhile_mpi_v10-1.out -lgmp
 * 
 *   	18014398241046527 - no dividers. To run the program say:
 *	 	time -p mpirun 4 ./referencefwhile_mpi_v10-1.out 18014398241046527 
 *
 *		I'm child, my number=1;
 *		I'm child, my number=0;
 *		I'm child, my number=3;
 *		I'm child, my number=2;
 *		divider number not found, thread=0
 *		divider number not found, thread=1
 *		divider number not found, thread=3
 *		divider number not found, thread=2
 *		real 6.09
 *		user 5.57
 *		sys 0.00
 *
 *
 *		9998000299980001 - one divider in the end of the last segment . To run the program say:
 *		time -p mpirun 4 ./referencefwhile_mpi_v10-1.out 9998000299980001		
 *  
 * 	I'm child, my number=3;
 *		I'm child, my number=0;
 *		I'm child, my number=2;
 *		I'm child, my number=1;
 *		divider number found - 99990001, thread=3
 *		application called MPI_Abort(MPI_COMM_WORLD, 0) - process 3
 *		real 4.41
 *		user 3.98
 *		sys 0.00
 *
 *		58745981236587 - one divider in the start of the first segment . To run the program say:
 *		time -p mpirun 4 ./referencefwhile_mpi_v10-1.out 58745981236587
 *
 *		I'm child, my number=3;
 *		I'm child, my number=2;
 *		I'm child, my number=1;
 *		I'm child, my number=0;
 *		divider number found - 3, thread=0
 *		application called MPI_Abort(MPI_COMM_WORLD, 0) - process 0
 *		real 0.15
 *		user 0.10
 *		sys 0.00
 *
 * author: Novohatko Nikita
 * date: 02.05.2019 - 09.06.2020
 */

//#include <omp.h> /* библиотека OPENMP */
#include <mpi.h> /* библиотека OPENMPI */
//#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gmp.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/ipc.h>
#include <linux/sem.h>
#include <pthread.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>

int is_prime(mpz_t orig_number, mpz_t single_quotient, mpz_t fork_number); 
mpz_t orig_number;      /* объявление переменной, для которой будет находиться делитель, числа с множественной (?) точностью */ // original number

int single_fork(int argc1, char *argv1[]);
int number_roots(mpz_t orig_number, mpz_t fork_number, mpz_t quotient);

mpz_t quotient;
mpz_t fork_number;
mpz_t single_fork_number;

int div_int = 0;
int *all_done;

void initialise_omp_shared();
bool process_read();
void process_write(int div_flag);

void initialise_mpi_shared(int fork_num);
int process_mpi_parent(int fork_num);

/**
 * Program entry point
 *
 * @param argc          Number of command-line parameters
 * @param argv          Array of command-line parameters: multiply-number, number of forks
 */

int main(int argc, char *argv[]) {

/* Проверка правильности введённых параметров */
if (argc < 2) {
       fprintf(stderr, "\nUsage: %s <string>\n\n", argv[0]);
       exit(EXIT_FAILURE);
    }

if (mpz_init_set_str(orig_number, argv[1], 0)) { //???
      fprintf(stderr, "\nInvalid orig_number!\n\n");
      exit(EXIT_FAILURE);
    }

/* Распараллеливание при помощи средств OPENMPI */

int rc;

rc = MPI_Init(&argc, &argv); /* Запуск параллельной части кода. Отсюда начинается параллельный код */ 

mpz_init(single_fork_number);
int myid, numprocs;

if (rc) {
	MPI_Abort(MPI_COMM_WORLD, rc);
}

MPI_Comm_size(MPI_COMM_WORLD, &numprocs); /* Получение количества процессов для группы MPI_COMM_WORLD в переменную numprocs */
MPI_Comm_rank(MPI_COMM_WORLD, &myid); /* Получение номера процесса для группы MPI_COMM_WORLD в переменную numprocs */

int i = myid; /* Получение номера потока */ 

printf("I'm child, my number=%d;\n", i);

mpz_init_set_si(fork_number, numprocs);	

/* Вычисление, с какого номера будут перебираться делители числа */
if (!number_roots(orig_number, fork_number, quotient)) {
      fprintf(stderr, "\nInvalid quotient!\n\n");
      exit(EXIT_FAILURE);   
	}

mpz_set_si(single_fork_number, i); /* присвоение переменной-делителю значения переменной старт */

if (!is_prime(orig_number, quotient, single_fork_number)) {
 		//printf("==singlefork omp composite== %d\n", i);
   } else {
		//printf("==singlefork omp prime== %d\n", i);   
   }

//printf("==multifork omp prime==\n");
	
MPI_Finalize(); /* Выход из параллельной части кода. Здесь заканчивается параллельный код */	

return 0;
}

/**
 * Find the least divider. The result is printed out on stdout.
 * If the divider is not found the function returns 1.
 *
 * @param orig_number   Source number
 * @return              0 - the divider was found,
 *                      1 - the divider was not found.
 */

int is_prime(mpz_t orig_number, mpz_t single_quotient, mpz_t fork_number) {
    mpz_t root, root_finish;
    mpz_t divider, divider_start;
    mpz_t remainder;
    mpz_t quotient;
    int start = 2;
    int step = 1;

	 int i; /* Номер потока */    
    i = mpz_get_si(fork_number); /* присвоение переменной типа интеджер значения количества потоков */    
    
    mpz_init(root); /* объявление переменной, в которой будет находиться корень, числа с множественной (?) точностью */
    mpz_init(root_finish); /* объявление переменной, в которой будет находиться корень, числа с множественной (?) точностью */
    mpz_init(divider); /* объявление переменной, в которой будет находиться делитель, числа с множественной (?) точностью */
    mpz_init(divider_start); /* объявление переменной, в которой будет находиться делитель, числа с множественной (?) точностью */
    mpz_init(remainder);
    mpz_init(quotient);
    
    mpz_set_si(divider_start, start); /* присвоение переменной-делителю значения переменной старт */

	 mpz_mul(divider, fork_number, single_quotient); /* умножение двух параметров и сохранение результата в divider */    
   
   /* Корректировка начала отрезка */
    if(mpz_cmp(divider, divider_start) == -1) 
   	mpz_set(divider, divider_start); /* присвоение переменной-делителю значения переменной старт */ 
 
    mpz_add_ui(fork_number, fork_number, 1);
    mpz_mul(root, fork_number, single_quotient); /* умножение двух параметров и сохранение результата в root */

	/* Корректировка конца отрезка */
	 mpz_sqrt(root_finish, orig_number); /* получение корня входной переменной */
	 if(mpz_cmp(root, root_finish) == 1) 
   	mpz_set(root, root_finish); /* присвоение переменной-делителю значения переменной старт */
    
    while (mpz_cmp(divider, root) <= 0) { /* mpz_cmp - сравнивает divider и root и выдаёт 1, если divider>root; 0, если divider==root; -1, если divider<root */
			
    	  mpz_cdiv_qr(quotient, remainder, orig_number, divider); /* mpz_cdiv_qr - получает частное quatient = orig_number/divider, остаток remainder = orig_number - divider*quotient */
        
        if (mpz_sgn(remainder) == 0) { /* mpz_sgn - получает 1, если divider>0; 0, если divider=0; -1, если divider<0 */
           printf("divider number found - %s, thread=%d\n", mpz_get_str(NULL, 10, divider), i); /* mpz_get_str - конвертирование divider в строку */

		  MPI_Abort(MPI_COMM_WORLD, 0);
           
        return 0;
        } else {
        mpz_add_ui(divider, divider, step); /* mpz_add_ui - суммирование divider = divider + step */
        }
    }
    printf("divider number not found, thread=%d\n", i); /* mpz_get_str - конвертирование divider в строку */
    return 1;
}

int number_roots(mpz_t orig_number, mpz_t fork_number, mpz_t quotient) {
	mpz_t root;
	mpz_t remainder;
   	
	mpz_init(root); /* объявление переменной, в которой будет находиться корень, числа с множественной (?) точностью */
	mpz_sqrt(root, orig_number); /* получение корня входной переменной */
	
	mpz_init(remainder);
	
	mpz_cdiv_qr(quotient, remainder, root, fork_number);	/* mpz_cdiv_qr - получает частное quatient = orig_number/fork_number, остаток remainder = orig_number - fork_number*quotient */

	return 3;	
	}