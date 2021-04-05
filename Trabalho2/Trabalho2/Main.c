#define _CRT_SECURE_NO_WARNINGS 1
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

#define SUCCESS 1
#define FAILURE -1

#define SLEEPING 0
#define WORKING 1

#define WAITING 0
#define ATTENDED 1

// Set TRUE or FALSE to show or not Debug Prints
#define DEBUG_MODE TRUE


#pragma region Structs
struct customer_pack;
struct barber_pack;

typedef struct struct_semaphore
{
	sem_t semaphore;
} struct_semaphore;

typedef struct customer_pack
{
	pthread_t thread_id;
	int id;

	struct barber_pack *array_barbers;
	int num_barbers;

	int status;
	struct_semaphore *chairs_customers; // Control the number of clients waiting the attend
	struct_semaphore *chairs_barbers; // Control the number of occuped barbers chair
	struct_semaphore *sleeping_barbers; // Control the number of sleeping barbers

	pthread_cond_t haircut_signal;
	pthread_mutex_t lock_helper;
} customer_pack;

typedef struct barber_pack
{
	pthread_t thread_id;
	int id;

	struct_semaphore *chairs_barbers;
	struct_semaphore *chairs_customers; // Control the number of clients waiting the attend
	struct customer_pack *customer; // Customer Instance

	pthread_cond_t time_to_work_signal;
	pthread_mutex_t lock_helper;

	int num_chairs;
	int status;
	int objective;
	int done;
} barber_pack;
#pragma endregion

#pragma region Fill Structs
void fill_barber_pack(barber_pack *array_barbers, struct_semaphore *chairs_barbers, struct_semaphore *chairs_customers, int num_barbers, int num_chairs, int min_attended)
{
	for (int i = 0; i < num_barbers; i++)
	{
		array_barbers[i].id = i;
		array_barbers[i].chairs_barbers = chairs_barbers;
		array_barbers[i].chairs_customers = chairs_customers;

		array_barbers[i].time_to_work_signal = PTHREAD_COND_INITIALIZER;
		array_barbers[i].lock_helper = PTHREAD_MUTEX_INITIALIZER;

		array_barbers[i].num_chairs = num_chairs;
		array_barbers[i].objective = min_attended;
		array_barbers[i].done = 0;
		array_barbers[i].status = SLEEPING;
	}
}
#pragma endregion

int val1, val2;
#pragma region Threads
void *customer(void *thread_pack)
{
	customer_pack *customer_pack = thread_pack; // customer instance
	int response_code;
	
	#if DEBUG_MODE
	printf("Customer %d: Thread Started. Memory %p\n", customer_pack->id, (void *)customer_pack->chairs_customers->semaphore);
	#endif

	// Try to get a waiting chair
	response_code = sem_trywait(&customer_pack->chairs_customers->semaphore);
	
	// If not avaible, go away
	if (response_code == FAILURE)
	{
		#if DEBUG_MODE
		printf("Customer %d: Fail to get a chair. The barbershop is full. %p\n", customer_pack->id, (void *) &customer_pack->chairs_customers->semaphore);
		#endif

		return NULL;
	}

	// Get Free chairs
	int free_chairs;
	sem_getvalue(&customer_pack->chairs_customers->semaphore, &free_chairs);

	#if DEBUG_MODE
	printf("Customer %d: Is waiting on a chair. Free Chairs: %d | Mem %p\n", customer_pack->id, free_chairs, (void *) & customer_pack->chairs_customers->semaphore);
	#endif

	// Verify if there is any barber sleeping, if it has, continue
	sem_wait(&customer_pack->sleeping_barbers->semaphore);

	// TODO:::: MAKE THIS BLOCK ATOMIC - Necessary for more than 1 barber
	// Lock to verify which one is sleeping

	int pos;
	barber_pack *array_barbers = customer_pack->array_barbers;
	for (int i = 0; i < customer_pack->num_barbers; i++) 
	{
		if (array_barbers[i].status == SLEEPING) {
			array_barbers[i].customer = customer_pack;
			array_barbers[i].status = WORKING;
			
			pos = i;
			#if DEBUG_MODE
			printf("Customer %d: Is going to be attended by Barber %d!\n", customer_pack->id, array_barbers[i].id);
			#endif

			break;
		}
	}

	// Unlock

	// Let a waiting chair free
	sem_post(&customer_pack->chairs_customers->semaphore);

	// Wakes the barber and get a haircut
	// sem_post(&customer_pack->chairs_barbers->semaphore);
	pthread_cond_signal(&array_barbers[pos].time_to_work_signal);

	printf("Customer %d: Signaling Barber %d\n", customer_pack->id, customer_pack->array_barbers[pos].id);

	// Wait signal until haircut is done
	pthread_mutex_lock(&customer_pack->lock_helper);
	pthread_cond_wait(&customer_pack->haircut_signal, &customer_pack->lock_helper);
	pthread_mutex_unlock(&customer_pack->lock_helper);

	#if DEBUG_MODE
	printf("Customer %d: Haircut is done! Leaving barbershop. %p\n", customer_pack->id, (void *)&customer_pack->chairs_customers->semaphore);
	#endif

	return NULL;
}

void *barber(void *thread_pack)
{
	barber_pack *barber_pack = thread_pack; // barber instance

	#if DEBUG_MODE
	printf("Barber %d: Thread Started\n", barber_pack->id);
	#endif

	int chairs_free;
	sem_getvalue(&barber_pack->chairs_customers->semaphore, &chairs_free);

	int barber_chair_occupied;
	sem_getvalue(&barber_pack->chairs_barbers->semaphore, &barber_chair_occupied);

	// while (barber_pack->done < barber_pack->objective || chairs_free < barber_pack->num_chairs || barber_chair_occupied > 0)
	while (TRUE)
	{
		printf("Barber %d: Is sleeping. Total done: %d #Locked \n", barber_pack->id, barber_pack->done);

		// Stay sleeping until a customer sends a signal
		pthread_mutex_lock(&barber_pack->lock_helper);
		pthread_cond_wait(&barber_pack->time_to_work_signal, &barber_pack->lock_helper);
		pthread_mutex_unlock(&barber_pack->lock_helper);

		printf("Signaled by: Customer %d \n", barber_pack->customer->id);

		// When wakes attend a customer
		printf("Barber %d: is attending Customer %d\n", barber_pack->id, barber_pack->customer->id);
		Sleep(50); // Time Cutting Hair - FOR TESTS ONLY

		// Tell to customer that the haircut is done. End it and back to sleep.
		barber_pack->status = SLEEPING;
		barber_pack->done += 1;
		pthread_cond_signal(&barber_pack->customer->haircut_signal);

		// Update the number of free customer chairs
		sem_getvalue(&barber_pack->chairs_customers->semaphore, &chairs_free);
		sem_getvalue(&barber_pack->chairs_barbers->semaphore, &barber_chair_occupied);

		// Tells that is going back to sleep and is not occupied
		sem_post(&barber_pack->customer->sleeping_barbers->semaphore);

		printf("Barber %d: Haircut is done on Customer %d \n", barber_pack->id, barber_pack->customer->id);
	}

	printf("Barber %d: Is sleeping. Total done: %d #Ended\n", barber_pack->id, barber_pack->done);
	printf(">>> BABEIRO FECHOU\n");

	return NULL;
}
#pragma endregion

int main(int argc, char **argv)
{
	int num_barbers, num_chairs, min_attended;

	printf("+ Started. +\n");

	// Get Inputed Values - should get from argv[]
	num_barbers = 1;
	num_chairs = 10;
	min_attended = 10;

	// Declare Semaphores
	struct_semaphore chairs_customers;
	struct_semaphore chairs_barbers;
	struct_semaphore sleeping_barbers;
	
	// Init Semaphores
	sem_init(&chairs_customers.semaphore, 1, num_chairs);
	sem_init(&chairs_barbers.semaphore, 1, 0);
	sem_init(&sleeping_barbers.semaphore, 1, num_barbers);

	#if DEBUG_MODE
		printf("Initialized Barber Semaphore. semaphore_chairs: %p\n", (void *) chairs_barbers.semaphore);
		printf("Initialized Customers Semaphore. Memory: %p\n", (void *) chairs_customers.semaphore);
	#endif

	//Initialize Barbers
	barber_pack *array_barbers = malloc(sizeof(barber_pack) * (num_barbers));
	fill_barber_pack(array_barbers, &chairs_barbers, &chairs_customers, num_barbers, num_chairs, min_attended);


	for (int i = 0; i < num_barbers; i++) 
	{
		int error_thread = pthread_create(&(array_barbers[i].thread_id), NULL, barber, &(array_barbers[i]));

		if (error_thread != 0)
		{
			printf("Fail to create Lab Thread %d\n, Error code: %d\n", array_barbers[i].id, error_thread);
			exit(-1);
		}
	}

	Sleep(1000);
	int num_clients = 100;
	for (int i = 0; i < num_clients; i++)
	{
		// Instanciate Customers
		customer_pack *customer_pack = malloc(sizeof(struct customer_pack));
		customer_pack->id = i;
		customer_pack->array_barbers = array_barbers;
		customer_pack->chairs_customers = &chairs_customers;
		customer_pack->chairs_barbers = &chairs_barbers;
		customer_pack->sleeping_barbers = &sleeping_barbers;
		customer_pack->num_barbers = num_barbers;
		customer_pack->haircut_signal = PTHREAD_COND_INITIALIZER;
		customer_pack->lock_helper = PTHREAD_MUTEX_INITIALIZER;

		// printf("Customer %d: customer_pack created. %p\n", customer_pack->id, (void *) customer_pack);

		Sleep(1);
		int error_thread = pthread_create(&(customer_pack->thread_id), NULL, customer, customer_pack);

		if (error_thread != 0)
		{
			printf("Fail to create Lab Thread %d\n, Error code: %d\n", customer_pack->id, error_thread);
			exit(-1);
		}
	}

	Sleep(5000);
	printf("+ End of program. +\n");

	return 0;
}
