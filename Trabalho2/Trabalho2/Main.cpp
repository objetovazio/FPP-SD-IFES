#define _CRT_SECURE_NO_WARNINGS 1
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <vector>
#include <random>

#define TRUE 1
#define FALSE 0

#define SUCCESS 0
#define FAILURE -1

#define SLEEPING 0
#define WORKING 1

#define WAITING 0
#define ATTENDED 1

// Set TRUE or FALSE to show or not Debug Prints
#define DEBUG_MODE FALSE

// This function is FOR TEST PURPOSE ONLY. This gets a semaphore and return its current value. This is used on prints.
int s_getvalue(sem_t *sem)
{
	int val1;
	sem_getvalue(sem, &val1);
	return val1;
}

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

	struct barber_pack *array_barbers;     // Array of Barbers 
	int num_barbers;                       // Size of array_barbers

	struct_semaphore *chairs_customers;    // Control the number of waiting chairs that are NOT occupied. Ex: If initialized value is 10, in a certain point the value is 6, means that 4 clients are waiting and 6 chairs are free.
	struct_semaphore *sleeping_barbers;    // Control the number of sleeping barbers. Ex: If value is 2, there are 2 barbers that are sleeping.
	pthread_mutex_t *barber_verifier;      // Mutex to block 1 customer to verify which barber is sleeping on array_barbers
	
	pthread_cond_t haircut_signal;         // Conditional to Signalize Customer that Haircut is Done
	pthread_mutex_t haircut_signal_helper; // Mutex that helps haircut_signal
	struct_semaphore status_attending;     // Control if the user is being attending and makes barber wait until customer is ready to be signaled

} customer_pack;

typedef struct barber_pack
{
	pthread_t thread_id;
	int id;

	struct_semaphore *chairs_customers;         // Control the number of waiting chairs that are NOT occupied. Ex: If initialized value is 10, in a certain point the value is 6, means that 4 clients are waiting and 6 chairs are free.
	struct_semaphore *sleeping_barbers;         // Control the number of sleeping barbers. Ex: If value is 2, there are 2 barbers that are sleeping.
	struct customer_pack *customer;             // Customer Instance - Reference to the customer that is going to be attended

	pthread_cond_t time_to_work_signal;         // Conditional to Signalize Barber that is time to work
	pthread_mutex_t time_to_work_signal_helper; // Mutex that helps time_to_work_signal

	struct_semaphore *barber_work_not_done;     // Control the number of barbers with work not done. If this arrives to 0, means that every barber finished his work.
	pthread_mutex_t *mutex_shared_objectives;   // Mutex that helps barber_work_not_done.

	struct_semaphore status_working;            // Control if a specific barber is working or sleeping. Busy = 0, sleeping = 1;
	int num_chairs;
	int num_barbers;
	int objective;
	int done;
	int finished;
} barber_pack;
#pragma endregion

#pragma region Fill Structs
// All barbers data
void fill_barber_pack(barber_pack *array_barbers, struct_semaphore *chairs_customers, struct_semaphore *sleeping_barbers, int num_barbers, int num_chairs, int min_attended, struct_semaphore *barber_work_not_done, pthread_mutex_t *mutex_shared_objectives)
{
	for (int i = 0; i < num_barbers; i++)
	{
		array_barbers[i].id = i;
		array_barbers[i].chairs_customers = chairs_customers;
		array_barbers[i].sleeping_barbers = sleeping_barbers;
		array_barbers[i].barber_work_not_done = barber_work_not_done;
		array_barbers[i].mutex_shared_objectives = mutex_shared_objectives;
		array_barbers[i].finished = FALSE;

		array_barbers[i].time_to_work_signal = PTHREAD_COND_INITIALIZER;
		array_barbers[i].time_to_work_signal_helper = PTHREAD_MUTEX_INITIALIZER;

		array_barbers[i].num_barbers = num_barbers;
		array_barbers[i].num_chairs = num_chairs;
		array_barbers[i].objective = min_attended;
		array_barbers[i].done = 0;

		sem_init(&array_barbers[i].status_working.semaphore, 0, 0); // Started BUSY (wake), but once the thread is created, should go to sleep.
	}
}
#pragma endregion

#pragma region Threads
void *customer(void *thread_pack)
{
	customer_pack *customer_pack = (struct customer_pack*) thread_pack; // customer instance
	int response_code;
	
	// Try to get a waiting chair
	response_code = sem_trywait(&(customer_pack->chairs_customers->semaphore));
	
	// If not avaible, go away
	if (response_code == FAILURE)
	{
		// Comented because the number of threads created is VERY BIG, and prints make the code slower.
		#if DEBUG_MODE
		//printf("Customer %d: Fail to get a chair. The barbershop is full. FC: %d, SB: %d --- thread_id.p: %p \n", customer_pack->id, s_getvalue(&customer_pack->chairs_customers->semaphore), s_getvalue(&customer_pack->sleeping_barbers->semaphore), customer_pack->thread_id.p);
		#endif

		pthread_cancel(customer_pack->thread_id);
		return NULL;
	}

	#if DEBUG_MODE
	// Get a free waiting chair
	printf("Customer %d: Is waiting on a chair. FC: %d, SB: %d \n", customer_pack->id, s_getvalue(&customer_pack->chairs_customers->semaphore), s_getvalue(&customer_pack->sleeping_barbers->semaphore));
	#endif

	// Verify if there is any barber sleeping, if it has, continue. Else, wait until a barber go back to sleep.
	sem_wait(&customer_pack->sleeping_barbers->semaphore);

	// Lock the access to the array_barbers and get a random barber and is sleeping at the moment.
	#pragma region Critical Region : barber_verifier
	barber_pack *array_barbers = customer_pack->array_barbers;
	
	// Lock to verify which one is sleeping
	pthread_mutex_lock(customer_pack->barber_verifier);

	// Start in a RANDOM BARBER and get one that is SLEEPING
	int pos = -1, i = customer_pack->num_barbers, rand_i;

	// Generate a RANDOM NUMBER in uniform distribution (This makes a HUGE difference, since the barber works very fast. If I look for a barber in sequence, I'll always get the first or second, causing STARVATION)
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, customer_pack->num_barbers - 1);

	// Look up for the first occurrence of a sleeping barber.
	rand_i = dis(gen);
	while (i) {
		response_code = sem_trywait(&array_barbers[rand_i].status_working.semaphore);

		// Found a sleepy barber.
		if (response_code == SUCCESS) {
			array_barbers[rand_i].customer = customer_pack;
			pos = rand_i;

			#if DEBUG_MODE
			printf("Customer %d: Is going to be attended by Barber %d! FC: %d, SB: %d \n", customer_pack->id, array_barbers[rand_i].id, s_getvalue(&customer_pack->chairs_customers->semaphore), s_getvalue(&customer_pack->sleeping_barbers->semaphore));
			#endif

			// The while will always be breaked, because the only way to get here is making sure that THERE ARE sleeping barbers with `customer_pack->sleeping_barbers->semaphore` AND the costumer finally found his position.
			break;
		}

		// Make rand_it back to 0 if passed the num of barbers
		if (rand_i + 1 >= customer_pack->num_barbers)
		{
			rand_i = 0;
		}
		else
		{
			rand_i += 1;
		}

		i--;
	}

	// Unlock barber_verifier
	pthread_mutex_unlock(customer_pack->barber_verifier);
	#pragma endregion

	// Set this flag to 0, making this customer busy while being attended by the barber
	sem_wait(&customer_pack->status_attending.semaphore); // Start being attended

	// ATTENTION: This is a barrier - Send a signal to wake the barber and get the fastest haircut in the world
	pthread_mutex_lock(&array_barbers[pos].time_to_work_signal_helper);
	pthread_cond_signal(&array_barbers[pos].time_to_work_signal);
	pthread_mutex_unlock(&array_barbers[pos].time_to_work_signal_helper);

	#if DEBUG_MODE
	printf("Customer %d: Signaling Barber %d! FC: %d, SB: %d \n", customer_pack->id, customer_pack->array_barbers[pos].id, s_getvalue(&customer_pack->chairs_customers->semaphore), s_getvalue(&customer_pack->sleeping_barbers->semaphore));
	#endif

	// ATTENTION: This is a barrier - This barrier keeps the customer here, waiting until receive signal until barber tells that the haircut is done
	pthread_mutex_lock(&customer_pack->haircut_signal_helper);
	sem_post(&customer_pack->status_attending.semaphore);
	while (pthread_cond_wait(&customer_pack->haircut_signal, &customer_pack->haircut_signal_helper) != 0);
	pthread_mutex_unlock(&customer_pack->haircut_signal_helper);

	#if DEBUG_MODE
	printf("Customer %d: Haircut is done! Leaving barbershop. FC: %d, SB: %d \n", customer_pack->id, s_getvalue(&customer_pack->chairs_customers->semaphore), s_getvalue(&customer_pack->sleeping_barbers->semaphore));
	#endif
	
	//pthread_exit(&response_code);
	return NULL;
}

void *barber(void *thread_pack)
{
	barber_pack *barber_pack = (struct barber_pack*) thread_pack; // barber instance

	#if DEBUG_MODE
	printf("Barber %d: Thread Started. FC: %d, SB: %d \n", barber_pack->id, s_getvalue(&barber_pack->chairs_customers->semaphore), s_getvalue(&barber_pack->sleeping_barbers->semaphore));
	#endif

	// The barber is always alive until the main process ends. I'll not handle to make it kill himself here.
	while (TRUE)
	{
		#if DEBUG_MODE
		printf("Barber %d: Is sleeping. Total done: %d FC: %d, SB: %d \n", barber_pack->id, barber_pack->done, s_getvalue(&barber_pack->chairs_customers->semaphore), s_getvalue(&barber_pack->sleeping_barbers->semaphore));
		#endif

		// Go to sleep and Add one more barber to the not working semaphore value.
		// Context Explanation: These variables are initialized with 0. And it represents:
		// status_working: 1 sleeping, 0 not sleeping. (about this barber instance)
		// If sleeping_barbers > 0: Someone is sleeping. (all barbers)
		sem_post(&barber_pack->status_working.semaphore);
		sem_post(&barber_pack->sleeping_barbers->semaphore);

		// ATTENTION: This is a barrier - Keep the barber sleeping here until customer sends a signal asking for a haircut.
		pthread_mutex_lock(&barber_pack->time_to_work_signal_helper);
		while (pthread_cond_wait(&barber_pack->time_to_work_signal, &barber_pack->time_to_work_signal_helper) != 0);
		pthread_mutex_unlock(&barber_pack->time_to_work_signal_helper);

		// Let a waiting chair FREE for new clients to sit.
		sem_post(&barber_pack->chairs_customers->semaphore);

		#if DEBUG_MODE
		// Attend a customer
		printf("Barber %d: Signaled by Customer %d. FC: %d, SB: %d \n", barber_pack->id, barber_pack->customer->id, s_getvalue(&barber_pack->chairs_customers->semaphore), s_getvalue(&barber_pack->sleeping_barbers->semaphore));
		#endif

		// Attend a customer, his haircut is pretty awesome now.
		barber_pack->done += 1;

		// Verify if barber completed his minimum objective.
		if(barber_pack->done == barber_pack->objective)
		{
			// Subtract this barber from the ones that still has to complete their objectives.
			// Context Explanation: barber_work_not_done is initialized with num_barbers.
			// if barber_work_not_done == 0, everyone completed their objective.
			// else, there are barbers that didnt complete yet.
			pthread_mutex_lock(barber_pack->mutex_shared_objectives);
			sem_wait(&barber_pack->barber_work_not_done->semaphore);
			barber_pack->finished = TRUE; // I was using this to set a Sleep() on who finished, but not using anymore. I'll keep it for tests purpose only. 
			pthread_mutex_unlock(barber_pack->mutex_shared_objectives);
		}

		// ATTENTION: This is a barrier - The most important barrier, this sends a signal to the customer saying that its all done.
		// The while is necessary to make sure that the signal will not be sent until customer is waiting for it. Once the custoemr could receive the signal,
		// the result code will let the process continue.
		int result_code;
		do {
			pthread_mutex_lock(&barber_pack->customer->haircut_signal_helper);
			pthread_cond_signal(&barber_pack->customer->haircut_signal);
			result_code = sem_trywait(&barber_pack->customer->status_attending.semaphore);
			pthread_mutex_unlock(&barber_pack->customer->haircut_signal_helper);
		} while (result_code != SUCCESS);

		#if DEBUG_MODE
		printf("Barber %d: Haircut is done on Customer %d. FC: %d, SB: %d \n", barber_pack->id, barber_pack->customer->id, s_getvalue(&barber_pack->chairs_customers->semaphore), s_getvalue(&barber_pack->sleeping_barbers->semaphore));
		#endif
	}

	return NULL;
}
#pragma endregion

int main(int argc, char *argv[])
{
	int num_barbers, num_chairs, min_attended;

	#if DEBUG_MODE
	printf("+ Started. +\n");
	#endif

	// Get Inputed Values - should get from argv[]
	num_barbers  = atoi(argv[1]); // 200;
	num_chairs   = atoi(argv[2]); // 1000;	
	min_attended = atoi(argv[3]); // 10;
	 
	// Declare Semaphores and mutexes
	struct_semaphore chairs_customers;
	struct_semaphore sleeping_barbers;
	struct_semaphore barber_work_not_done;
	pthread_mutex_t barber_verifier;
	pthread_mutex_t mutex_shared_objectives;

	// Init Semaphores and mutexes
	sem_init(&chairs_customers.semaphore, 0, num_chairs); // Represents the waiting chairs, where the customers can sit on the barbershop.
	sem_init(&sleeping_barbers.semaphore, 0, 0);		  // Init waked up, but once the thread is created, should go to sleep.
	sem_init(&barber_work_not_done.semaphore, 0, num_barbers); // Init with num_barbers, and will be decremented once someone finish its work.
	barber_verifier = PTHREAD_MUTEX_INITIALIZER;
	mutex_shared_objectives = PTHREAD_MUTEX_INITIALIZER;

	#if DEBUG_MODE
	printf("Initialized Barber Semaphore. sleeping_barbers: %p\n", (void *) sleeping_barbers.semaphore);
	printf("Initialized Customers Semaphore. chairs_customers: %p\n", (void *) chairs_customers.semaphore);
	#endif

	//Initialize Barbers
	barber_pack *array_barbers = (struct barber_pack*) malloc(sizeof(barber_pack) * (num_barbers));
	fill_barber_pack(array_barbers, &chairs_customers, &sleeping_barbers, num_barbers, num_chairs, min_attended, &barber_work_not_done, &mutex_shared_objectives);

	// Start Barbers threads
	std::vector<pthread_t> barber_thread_id;
	for (int i = 0; i < num_barbers; i++) 
	{
		int error_thread = pthread_create(&(array_barbers[i].thread_id), NULL, barber, &(array_barbers[i]));

		// Saves Barber Thread Id
		barber_thread_id.push_back(array_barbers[i].thread_id);

		if (error_thread != 0)
		{
			printf("Fail to create Barber Thread %d\n, Error code: %d\n", array_barbers[i].id, error_thread);
			exit(-1);
		}
	}

	// Instantiate and create customers threads
	std::vector<pthread_t> customer_thread_id;
	int error_thread, response_code, j = 0, create_customer = num_barbers;

	// Create customers until all barbers didn't accomplish their objective.
	while(create_customer)
	{
		// Instanciate Customers
		customer_pack *customer_pack = (struct customer_pack*) malloc(sizeof(struct customer_pack));
		customer_pack->id = j;
		customer_pack->array_barbers = array_barbers;
		customer_pack->chairs_customers = &chairs_customers;
		customer_pack->sleeping_barbers = &sleeping_barbers;
		customer_pack->num_barbers = num_barbers;
		customer_pack->haircut_signal = PTHREAD_COND_INITIALIZER;
		customer_pack->haircut_signal_helper = PTHREAD_MUTEX_INITIALIZER;
		customer_pack->barber_verifier = &barber_verifier;
		sem_init(&customer_pack->status_attending.semaphore, 0, 1); // Value 1 Not being attended, Value 0 Being attended
		
		// Verify if there are barbers working. The mutex is to make sure that the value dont change in the middle of a thread creation.
		pthread_mutex_lock(&mutex_shared_objectives);
		response_code = sem_trywait(&barber_work_not_done.semaphore);
		if (response_code == SUCCESS) {
			error_thread = pthread_create(&(customer_pack->thread_id), NULL, customer, customer_pack);
			customer_thread_id.push_back(customer_pack->thread_id);
			sem_post(&barber_work_not_done.semaphore);
		}

		create_customer = s_getvalue(&barber_work_not_done.semaphore);
		pthread_mutex_unlock(&mutex_shared_objectives);

		if (error_thread != 0)
		{
			printf("Fail to create Customer Thread %d\n, Error code: %d\n", customer_pack->id, error_thread);
			exit(-1);
		}
		
		j++; // Customer id ++
	}

	#if DEBUG_MODE
	printf(">>> Attention, Barbershop is closed!\n");
	#endif
	
	// Join all customers threads from start to end, poping the joined ones.
	// Context: I made it poping the ids because this way I was able to check ONLY the thread id from alive threads.
	int total = customer_thread_id.size();
	while (customer_thread_id.size()) {
		error_thread = pthread_join(customer_thread_id[0], NULL);
		customer_thread_id.erase(customer_thread_id.begin());

		if (error_thread != 0)
		{
			printf("Fail to receive Customer Thread %d, Error code: %d\n", 0, error_thread);
			exit(-2);
		}
	}

	#if DEBUG_MODE
	printf("+ End of program. +\n");
	#endif

	// Print the barbers works =)
	for (int i = 0; i < num_barbers; i++)
	{
		printf("Barber %3d: Attended %d Customers.\n", array_barbers[i].id, array_barbers[i].done);
	}

	return 0;
}
