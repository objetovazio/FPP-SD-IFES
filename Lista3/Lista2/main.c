#define _CRT_SECURE_NO_WARNINGS 1 
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1 

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct thread_data
{
	pthread_t id;
	int thread_number;
	unsigned long long start;
	unsigned long long end;
	unsigned long long partial_sum;
	int *vector;

	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	int *thread_counter;
	int *total_of_threads;
	clock_t *time;
} thread_data;

void fill_vector(int *vector, unsigned long long size_vector)
{
	for (unsigned long long i = 0; i < size_vector; i++)
	{
		vector[i] = rand() % 100000;
	}
}

void *thread_summation(void *thread_pack)
{
	thread_data *pack = thread_pack;
	unsigned long long sum_thread = 0;

	pthread_mutex_lock(pack->mutex);
	(*pack->thread_counter)++;
	// printf("Thread %d locked the mutex. Total %d \n", *pack->thread_counter, *pack->total_of_threads);
	if (*pack->thread_counter == *pack->total_of_threads)
	{
		// printf("Thread %d Arrived - Last Thread \n", pack->thread_number);
		*pack->thread_counter = 0;
		*pack->time = clock();
		pthread_cond_broadcast(pack->cond);
	}
	else while (pthread_cond_wait(pack->cond, pack->mutex) != 0);
	pthread_mutex_unlock(pack->mutex);

	// printf("Thread %d is processing from %lld to %lld \n", pack->thread_number, pack->start, pack->end);
	for (unsigned long long i = pack->start; i < pack->end; i++)
		sum_thread += pack->vector[i];

	// printf("Thread %d finished\n", pack->thread_number);
	pack->partial_sum = sum_thread;
}

void calculate_serial(unsigned long long size_vector, int *vector)
{
	unsigned long long i, sum_vector = 0;
	clock_t time;

	/* Serial */
	time = clock();
	for (i = 0; i < size_vector; i++)
	{
		sum_vector += vector[i];
	}
	time = clock() - time;
	printf("The sum of %lld elements in Serial Mode took %ld clicks\nFinal result: %lld\n", size_vector, time, sum_vector);

	FILE *log = NULL;
	log = fopen("./results_serial.log", "a");

	if (log == NULL)
	{
		printf("Error! can't open log file.");
		return;
	}

	fprintf(log, "%2d \t\t %10ld \t %20lld\n", 1, time, size_vector);
	fclose(log);
}

int calculate_thread(unsigned long long size_vector, int *vector, int number_threads)
{
	/* Declaring Variables */
	int error_thread, thread_counter;
	unsigned long long i, sum_vector, block_size;
	clock_t time;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	/* Setting inicial Value */
	sum_vector = 0;
	error_thread = 0;
	thread_counter = 0;
	block_size = size_vector / number_threads;

	/* Instantiate Semaphores and Malloc Threads Packages Array */
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	thread_data *thread_vector = malloc(sizeof(thread_data) * number_threads);

	/* Populate Thread Packages */
	for (i = 0; i < number_threads; i++)
	{
		thread_vector[i].thread_number = i;
		thread_vector[i].start = block_size * i;

		if (i == number_threads - 1)
		{
			thread_vector[i].end = size_vector;
		}
		else
		{
			thread_vector[i].end = block_size * (i + 1);
		}

		thread_vector[i].vector = vector;
		thread_vector[i].thread_counter = &thread_counter;
		thread_vector[i].mutex = &mutex;
		thread_vector[i].cond = &cond;
		thread_vector[i].time = &time; 
		thread_vector[i].total_of_threads = &number_threads;
		// printf("The thread %lld will process from %lld to %lld\n", thread_vector[i].thread_number, thread_vector[i].start, thread_vector[i].end);
	}

	for (i = 0; i < number_threads; i++)
	{
		error_thread = pthread_create(&(thread_vector[i].id), NULL, thread_summation, &(thread_vector[i]));

		if (error_thread != 0)
		{
			// printf("Fail to create Thread %lld\n", i);
			exit(-2);
		}
	}

	for (i = 0; i < number_threads; i++)
	{
		error_thread = pthread_join(thread_vector[i].id, NULL);

		if (error_thread != 0)
		{
			// printf("Fail to receive Thread %lld\n", i);
			exit(-3);
		}

		sum_vector += thread_vector[i].partial_sum;

		// printf("Thread %lld closed\n", i);
	}
	time = clock() - time;

	printf("The sum of %lld elements in Multithread Mode took %ld clicks\nFinal result: %lld\n", size_vector, time	, sum_vector);

	FILE *log = NULL;
	log = fopen("./results_thread.log", "a");

	if (log == NULL)
	{
		printf("Error! can't open log file.");
		return -1;
	}

	fprintf(log, "%2d \t\t %10ld \t %20lld\n", number_threads, time	, size_vector);
	fclose(log);

	free(thread_vector);
	return 0;
}

int main(int argc, char **argv)
{
	int number_threads;
	unsigned long long size_vector;
	char *eptr;

	/* */
	size_vector = strtoull(argv[1], &eptr, 10);
	number_threads = atoi(argv[2]);
	int *vector = malloc(sizeof(int) * size_vector);

	printf("### Details\n- Threads: %d\n- Vector Size: %lld\n\n", number_threads, size_vector);

	fill_vector(vector, size_vector);

	printf("### Starting Serial Mode\n");
	calculate_serial(size_vector, vector);

	printf("\n### Starting Multithread Mode\n");
	calculate_thread(size_vector, vector, number_threads);

	free(vector);

	return 0;
}

// $ gcc -Wall -pedantic -o main.exe main.c -lpthread
