/**
 * The program is proposed to find out whether the specified number is prime or not.
 * The source number is provided as first command-line parameter.
 * The result is printed out on stdout.
 *
 * To compile the program say:
 *    gcc -Wall referencefwhile_openmp_v4-1.c -o referencefwhile_openmp_v4-1.out -lgmp -fopenmp
 * 
 *   	18014398241046527 - no dividers. To run the program say:
 *	 	time -p ./referencefwhile_openmp_v4-1.out 18014398241046527 4 
 *
 *		I'm parent 0, my pid=2819;
 *		I'm child, my number=1;
 *		I'm child, my number=0;
 *		I'm child, my number=3;
 *		I'm child, my number=2;
 *		==singlefork omp prime== 1
 *		==singlefork omp prime== 3
 *		==singlefork omp prime== 0
 *		==singlefork omp prime== 2
 *		==multifork omp prime==
 *		real 7.53
 *		user 7.12
 *		sys 0.00
 *
 *		9998000299980001 - one divider in the end of the last segment . To run the program say:
 *		time -p ./referencefwhile_openmp_v4-1.out 9998000299980001 4		
 *  
 * 	I'm parent 0, my pid=2814;
 *		I'm child, number=0; 
 *		I'm child, number=3;
 *		I'm child, number=2;
 *		I'm child, number=1;
 *		==singlefork omp prime== 1
 *		99990001
 *		==singlefork omp composite== 3
 *		==singlefork omp prime== 0
 *		==singlefork omp composite== 2
 *		==multifork omp prime==
 *		real 5.49
 * 	user 5.22
 *		sys 0.00
 *
 *		58745981236587 - one divider in the start of the first segment . To run the program say:
 *		time -p ./referencefwhile_openmp_v4-1.out 58745981236587 4
 *
 *		I'm parent 0, my pid=2823;
 *		I'm child, number=0;
 *		3
 *		==singlefork omp composite== 0
 *		I'm child, number=3;
 *		==singlefork omp composite== 3
 *		I'm child, number=2;
 *		==singlefork omp composite== 2
 *		I'm child, number=1;
 *		==singlefork omp composite== 1
 *		==multifork omp prime==
 *		real 0.00
 * 	user 0.00
 *		sys 0.00
 *
 *
 * author: Novohatko Nikita
 * date: 02.05.2019 - 15.05.2020
 */

#include <omp.h> /* библиотека OPENMP */

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

omp_lock_t simple_lock;
bool done = false;

void initialise_omp_shared();
bool process_read();
void process_write(bool div_flag);


/**
 * Program entry point
 *
 * @param argc          Number of command-line parameters
 * @param argv          Array of command-line parameters: multiply-number, number of forks
 */

int main(int argc, char *argv[]) {

/* Инициализация мьютекса */
//initialise_omp_shared();

/*  Расположим наши данные в общей памяти */

printf("I'm parent 0, my pid=%d;\n", getpid());

/* Проверка правильности введённых параметров */
if (argc < 3) {
       fprintf(stderr, "\nUsage: %s <string>\n\n", argv[0]);
       exit(EXIT_FAILURE);
    }

if (mpz_init_set_str(orig_number, argv[1], 0)) {
      fprintf(stderr, "\nInvalid orig_number!\n\n");
      exit(EXIT_FAILURE);
    }
    
if (mpz_init_set_str(fork_number, argv[2], 0)) {
      fprintf(stderr, "\nInvalid fork_number!\n\n");
      exit(EXIT_FAILURE);
    }    

/* Вычисление, с какого номера будут перебираться делители числа */
if (!number_roots(orig_number, fork_number, quotient)) {
      fprintf(stderr, "\nInvalid quotient!\n\n");
      exit(EXIT_FAILURE);   
	}

int fork_num = atoi(argv[2]); /* преобразование генерируемого количества процессов из строки в число */
mpz_init(single_fork_number);

omp_set_num_threads(fork_num); /* Задание количества потоков для библиотеки OPENMP */

omp_init_lock(&simple_lock); /* Инициализация мьютекса средствами библиотеки OPENMP */

/* Распараллеливание при помощи средств OPENMP */

#pragma omp parallel
{
	int i = omp_get_thread_num(); /* Получение номера потока */
	printf("I'm child, my number=%d;\n", i);

	mpz_set_si(single_fork_number, i); /* присвоение переменной-делителю значения переменной старт */

				if (!is_prime(orig_number, quotient, single_fork_number)) {
      				printf("==singlefork omp composite== %d\n", i);
     		   } else {
     					printf("==singlefork omp prime== %d\n", i);   
     		   }
}

#pragma omp taskwait
	
	printf("==multifork omp prime==\n");
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
    bool div_flag = false;

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
			
			/* Считать потоково-защищённую переменную и проверить, если она true - завершить, если false - продолжать */
		  div_flag = process_read();
		  	  
		  if (div_flag == true)  {
				return 0;		  
		  }      
       
        mpz_cdiv_qr(quotient, remainder, orig_number, divider); /* mpz_cdiv_qr - получает частное quatient = orig_number/divider, остаток remainder = orig_number - divider*quotient */
        if (mpz_sgn(remainder) == 0) { /* mpz_sgn - получает 1, если divider>0; 0, если divider=0; -1, если divider<0 */
            printf("\t%s\n", mpz_get_str(NULL, 10, divider)); /* mpz_get_str - конвертирование divider в строку */
 
 		  /* Записать в потоково-защищённую переменную true */
		  process_write(true);
		             
        return 0;
        } else {
            mpz_add_ui(divider, divider, step); /* mpz_add_ui - суммирование divider = divider + step */
        }
    }
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
	
bool process_read() {
	bool div_flag;
	omp_set_lock(&simple_lock);
   div_flag = done;
   omp_unset_lock(&simple_lock);

	return div_flag;
}

void process_write(bool div_flag) {
	omp_set_lock(&simple_lock);
	done = true;
	omp_unset_lock(&simple_lock);
}
	

/* Создаётся имитация записи и чтения в файл, который служит буфером между процессами, так как обмен между ними защищён, в отличие от потоков */

void initialise_omp_shared() {
	/*  Расположим наши данные в общей памяти */
	omp_init_lock(&simple_lock); /* Инициализация мьютекса средствами библиотеки OPENMP */
	done = false;
}
