/**
 * The program is proposed to find out whether the specified number is prime or not.
 * The source number is provided as first command-line parameter.
 * The result is printed out on stdout.
 *
 * To compile the program say:
 *    gcc -Wall referencefwhile_v12-2.c -o referencefwhile_v12-2.out -lgmp -pthread
 * 
 *   	18014398241046527 - no dividers. To run the program say:
 *	 	time -p ./referencefwhile_v12-2.out 18014398241046527 4 
 *
 *		I'm parent 0, my pid=5281;
 *		I'm child, my pid=5285;
 *		I'm child, my pid=5284;
 *		I'm child, my pid=5283;
 *		I'm child, my pid=5282;
 *		==singlefork prime== 3
 *		==singlefork prime== 0
 *		==singlefork prime== 1
 *		==singlefork prime== 2
 *		==multifork prime==
 *		real 8.03
 *		user 7.65
 *		sys 0.00
 *
 *		9998000299980001 - one divider in the end of the last segment . To run the program say:
 *		time -p ./referencefwhile_v12-2.out 9998000299980001 4		
 *  
 * 	I'm parent 0, my pid=5331;
 *		I'm child, my pid=5335; 
 *		I'm child, my pid=5334;
 *		I'm child, my pid=5333;
 *		I'm child, my pid=5332;
 *		==singlefork prime== 0
 *		99990001
 *		==singlefork prime== 1
 *		==singlefork composite== 3
 *		==singlefork composite== 2
 *		==multifork prime==
 *		real 6.21
 * 	user 5.67
 *		sys 0.00
 *
 *		58745981236587 - one divider in the start of the first segment . To run the program say:
 *		time -p ./referencefwhile_v12-2.out 58745981236587 4
 *
 *		I'm parent 0, my pid=5397;
 *		I'm child, my pid=5399;
 *		I'm child, my pid=5400;
 *		I'm child, my pid=5398;
 *		3
 *		I'm child, my pid=5401;
 *		==singlefork composite== 0
 *		==singlefork composite== 2
 *		==singlefork composite== 1
 *		==singlefork composite== 3
 *		==multifork prime==
 *		real 0.01
 *		user 0.00
 *		sys 0.00
 *
 *
 * author: Novohatko Nikita
 * date: 14.12.2019 - 02.05.2020
 */

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

typedef struct
{
	bool done;
	pthread_mutex_t mutex;
} shared_data;

static shared_data* data = NULL;

bool process_read();
void process_write(bool div_flag);
void process_lock();
void process_unlock();
void initialise_shared();


/**
 * Program entry point
 *
 * @param argc          Number of command-line parameters
 * @param argv          Array of command-line parameters: multiply-number, number of forks
 */

int main(int argc, char *argv[]) {

/*
 * Create var tipe pid_t for writing number of child process
 * Создание переменной типа pid_t для записи номера дочернего процесса 
 */
pid_t pid;

int i;

/* Инициализация мьютекса */
initialise_shared();

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
int status[fork_num]; /* Создание массива со статусами дочерних процессов */

for (i = 0; i < fork_num; i++) {
	
	pid = fork();
			
	if (pid < 0) {
			perror("fork 1 error");
			exit(-1);
	}
			
	if (pid == 0) {
		   printf("I'm child, my pid=%d;\n", getpid());
		   
		   	mpz_set_si(single_fork_number, i); /* присвоение переменной-делителю значения переменной старт */

				if (!is_prime(orig_number, quotient, single_fork_number)) {
      				printf("==singlefork composite== %d\n", i);
      				exit(EXIT_SUCCESS);
    		   }
				
			printf("==singlefork prime== %d\n", i);
			return 0;
	}

	}
	
	for (i = 0; i < fork_num; i++) {	
		wait(&status[i]); /* Ожидание завершения дочерних процессов */
	}
	
	printf("==multifork prime==\n");
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
	process_lock();
	div_flag = data->done;
	process_unlock();

	return div_flag;
}

void process_write(bool div_flag) {
	process_lock();
	data->done = div_flag;
   process_unlock();
}
	
void process_lock() {
/* Заблокировать ресурс: */
	int ret;
   ret = pthread_mutex_lock(&data->mutex);
	if (ret != 0)
		exit(EXIT_FAILURE);
}

void process_unlock() {
/* Разблокировать ресурс: */
	int ret;
   ret = pthread_mutex_unlock(&data->mutex);
	if (ret != 0)
		exit(EXIT_FAILURE);
}

/* Создаётся имитация записи и чтения в файл, который служит буфером между процессами, так как обмен между ними защищён, в отличие от потоков */

void initialise_shared() {
	/*  Расположим наши данные в общей памяти */
	int prot = PROT_WRITE;
	int flags = MAP_SHARED | MAP_ANONYMOUS;
	data = mmap(NULL, sizeof(shared_data), prot, flags, -1, 0); /* системный вызов, отображающий файл с диска на память */	
	assert(data);
	
	data->done = false;
	
	/*  Проинициализируем мьютекс в общей памяти */
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&data->mutex,&attr);
}