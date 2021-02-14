#define _CRT_SECURE_NO_WARNINGS 1
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define TRUE 1
#define FALSE 0

// Set TRUE or FALSE to show or not Debug Prints
#define DEBUG_MODE FALSE

#pragma region Structs
typedef struct supply
{
	int id;					// Supply ID
	int type;				// Supply Type
	sem_t semaphore_supply; // Supply Semaphore to represents if it's avaiable to be collected
} supply;

typedef struct laboratory_data
{
	pthread_t thread_id;
	int lab_id;

	// Shared Properties
	pthread_mutex_t *mutex_table; // Mutex to lock the table of supplies
	supply *supply_table;		  // Table of supplies that the lab will lock when verifying it's production
	int size_of_supplies;		  // Size of the table of supplies

	pthread_mutex_t *mutex_shared_objectives; // Mutex to lock the table of supplies
	int *shared_objectives;					  // Share objective counts how many Infected and Laboratory ended it works
	int size_of_labs; // Helpers to know when all labs ended its work
	int size_of_infected; // Helpers to know when all infs ended its work

	// Lab Properties
	int *supplies_production_position; // Array of pointers to which supplies the lab will produce
	int supplies_production_size;	   // Total of supplies that lab will produce
	int total_restocking;			   // Total full restocking supplies (should count +1 when create new supplies)
	int total_restocking_objective;	   // Total of restocking that a lab should achieve at least
} laboratory_data;

typedef struct infected_data
{
	pthread_t thread_id;
	int inf_id;

	// Shared Properties
	pthread_mutex_t *mutex_table; // Mutex to lock the table of supplies
	supply *supply_table;		  // Table of supplies that the lab will lock when verifying it's production
	int size_of_supplies;		  // Size of the table of supplies

	pthread_mutex_t *mutex_shared_objectives; // Mutex to lock the table of supplies
	int *shared_objectives;					  // Share objective counts how many Infected and Laboratory ended it works
	int size_of_labs; // Helpers to know when all labs ended its work
	int size_of_infected; // Helpers to know when all infs ended its work

	// Infected Properties
	int *supplies_type_needed;	   // Type of supplies needed by the user
	int supplies_type_needed_size; // Type of supplies needed by the user
	int total_vaccines;			   // Total of times that infected got a vaccine
	int total_vaccines_objective;  // Total of restocking that a lab should achieve at least
} infected_data;

#pragma endregion

#pragma region Fill Structs
/*
* Fill the supplies semaphores, preparing it to start the threads.
* Supplies MUST start with no production (Semaphore starts as 0)
*/
void fill_supplies_data(
	supply *array_supplies,
	int size_of_supplies,
	int total_supplies_by_lab)
{
	int supply_type = 0;

	for (int i = 0; i < size_of_supplies * total_supplies_by_lab; i++)
	{
		array_supplies[i].id = i;
		array_supplies[i].type = supply_type + 1;

		sem_init(&array_supplies[i].semaphore_supply, 0, 0);

		if (supply_type == total_supplies_by_lab)
			supply_type = 0;
		else
			supply_type++;
	}
}

/*
* Fill the laboratories structs, preparing it to start the threads
*/
void fill_laboratory_data(
	laboratory_data *array_labs,
	int size_of_labs,
	supply *array_supplies,
	int size_of_supplies,
	int total_supplies_by_lab,
	int total_restocking_objective,
	pthread_mutex_t *mutex_table,
	int shared_objectives[2],
	pthread_mutex_t *mutex_shared_objectives,
	int size_of_infected)
{
	int supplies_index = 0;
	for (int i = 0; i < size_of_labs; i++)
	{
		array_labs[i].lab_id = i;
		array_labs[i].mutex_table = mutex_table;
		array_labs[i].supply_table = array_supplies;
		array_labs[i].size_of_supplies = size_of_supplies * total_supplies_by_lab;
		array_labs[i].total_restocking = 0;
		array_labs[i].total_restocking_objective = total_restocking_objective;

		array_labs[i].shared_objectives = shared_objectives;
		array_labs[i].mutex_shared_objectives = mutex_shared_objectives;
		array_labs[i].size_of_infected = size_of_infected;
		array_labs[i].size_of_labs = size_of_labs;

		// Malloc supplies array dinamically
		array_labs[i].supplies_production_position = malloc(sizeof(int) * total_supplies_by_lab);
		array_labs[i].supplies_production_size = total_supplies_by_lab;

		// Set to supplies_production_position the POSITION of Supplies on the table that the lab will produce (the semaphores) PS: Attencion to the "++" operator
		int j = 0;
		while (j < total_supplies_by_lab)
		{
			array_labs[i].supplies_production_position[j++] = supplies_index++; // Set the position of supply. Add +1 to "j" and "supplies_index" AFTER the command runs.
		}
	}
}

/*
* Fill the infecteds structs, preparing it to start the threads
*/
void fill_infected_data(
	infected_data *array_inf,
	int size_of_infected,
	supply *array_supplies,
	int size_of_supplies,
	int total_supplies_needed_infected,
	int total_vaccines_objective,
	pthread_mutex_t *mutex_table,
	int shared_objectives[2],
	pthread_mutex_t *mutex_shared_objectives,
	int size_of_labs)
{
	int supplies_type_index = 0; // Represent the index of supply type. Its used to rotate the Types.
	for (int i = 0; i < size_of_infected; i++)
	{
		array_inf[i].inf_id = i;
		array_inf[i].mutex_table = mutex_table;
		array_inf[i].supply_table = array_supplies;
		array_inf[i].size_of_supplies = size_of_supplies * total_supplies_needed_infected;
		array_inf[i].total_vaccines = 0;
		array_inf[i].total_vaccines_objective = total_vaccines_objective;

		array_inf[i].shared_objectives = shared_objectives;
		array_inf[i].mutex_shared_objectives = mutex_shared_objectives;

		// Malloc supplies array dinamically
		array_inf[i].supplies_type_needed = malloc(sizeof(int) * total_supplies_needed_infected);
		array_inf[i].supplies_type_needed_size = total_supplies_needed_infected;
		array_inf[i].size_of_infected = size_of_infected;
		array_inf[i].size_of_labs = size_of_labs;

		// Set to supplies_type_needed the Type that the infected have to find on the table
		int j = 0;
		while (j < total_supplies_needed_infected)
		{
			array_inf[i].supplies_type_needed[j] = (supplies_type_index + 1);

			// Rotates the supplies_index to the max, and then back to 0;
			if (supplies_type_index == size_of_supplies - 1)
				supplies_type_index = 0;
			else
				supplies_type_index++;

			j++;
		}
	}
}
#pragma endregion

#pragma region Threads
void *laboratory(void *thread_pack)
{
	laboratory_data *lab_pack = thread_pack; // Laboratory instance

	// Auxiliar Variables
	supply *supply_helper; // A pointer to the actual Supply on Loopings
	int lab_id = lab_pack->lab_id; // Lab Identification
	int i, emptySupplies; // Counter and helper to use as Boolean
	int supply_val, supply_old_val, supply_position_table; // Supply Helper Identifiers to Printing

	// Temporary
	int counterTest = 1000; // Temporary
	// if (lab_id != 2) {
	// 	//printf("> LAB %d: Terminated \n", lab_id);
	// 	return (void*)lab_pack; // Temporary
	// }

	do
	{
		#if DEBUG_MODE
		printf("> Lab %d: Objective Status: Done %d | LAB: %d | INF: %d \n", lab_id, lab_pack->total_restocking, lab_pack->shared_objectives[0], lab_pack->shared_objectives[1]);
		#endif

		// If already completed the objective, wait 10 miliseconds before continue.
		if (lab_pack->total_restocking > lab_pack->total_restocking_objective) {
			Sleep(10);
		}

		emptySupplies = TRUE;

		// Verify if need reestock without LOCKING TABLE
		for (i = 0; i < lab_pack->supplies_production_size; i++)
		{
			supply_position_table = lab_pack->supplies_production_position[i]; // Gets the position of the first supply that will be checked
			supply_helper = &lab_pack->supply_table[supply_position_table];	   // Sets the instance of the supply to supply_helper

			sem_getvalue(&supply_helper->semaphore_supply, &supply_val); // Gets the actual value of the Semaphore and set it to supply_val. 0: Supply Unavaiable, 1: Supply Avaiable.

			#if DEBUG_MODE
			printf("> LAB %d: is checking Supply[ID:%d][Type:%d] has Value %d SEM_MEM: %p \n", lab_id, supply_helper->id, supply_helper->type, supply_val, supply_helper->semaphore_supply);
			#endif

			if (supply_val == 1)
			{
				emptySupplies = FALSE;
				break;
			}
		}

		// If the supplies are not full empty, restart looping
		if (!emptySupplies)
		{
			counterTest--;
			continue;
		}

		// If the supplies of this LAB are empty, there is no why to check again. Only this lab can fill its own supplies.
		// Jumping directly to reestocking part.

		// Lock table to fill the supplies again without concorrences
		pthread_mutex_lock(lab_pack->mutex_table); // START OF CRITICAL REGION - supply_table

		// If all of them are empty, create new ones
		for (i = 0; i < lab_pack->supplies_production_size; i++)
		{
			supply_position_table = lab_pack->supplies_production_position[i]; // Gets the position of the first supply that will be checked
			supply_helper = &lab_pack->supply_table[supply_position_table];	   // Sets the instance of the supply to supply_helper

			// Just get the old value and print on debug mode, else just post the semaphore value
			#if DEBUG_MODE
			sem_getvalue(&supply_helper->semaphore_supply, &supply_old_val);
			sem_post(&supply_helper->semaphore_supply);
			sem_getvalue(&supply_helper->semaphore_supply, &supply_val);
			printf("> LAB %d: reestocking Supply[ID:%d][Type:%d] from: %d > to: %d - SEM_MEM: %p \n", lab_id, supply_helper->id, supply_helper->type, supply_old_val, supply_val, supply_helper->semaphore_supply);
			#else
			sem_post(&supply_helper->semaphore_supply);
			#endif
		}
		pthread_mutex_unlock(lab_pack->mutex_table); // END OF CRITICAL REGION - supply_table

		// Count one more reestock for this lab
		lab_pack->total_restocking++;

		// If this lab completed the reestocking objective, add one to shared_objectives
		if (lab_pack->total_restocking == lab_pack->total_restocking_objective)
		{
			pthread_mutex_lock(lab_pack->mutex_shared_objectives);  // START OF CRITICAL REGION - mutex_shared_objectives
			lab_pack->shared_objectives[0]++; // Add one lab that finished their objective
			pthread_mutex_unlock(lab_pack->mutex_shared_objectives); // END OF CRITICAL REGION - mutex_shared_objectives

			printf("> Lab %d: Objective Completed. Labs Completed: %d | Infected Completed: %d \n", lab_id, lab_pack->shared_objectives[0], lab_pack->shared_objectives[1]);
		}

	} while (lab_pack->shared_objectives[0] != lab_pack->size_of_labs || lab_pack->shared_objectives[1] != lab_pack->size_of_infected);

	printf("> Lab %d: Stopped. Total Reestock: %d - Objectives in LAB: %d | INF: %d \n", lab_id, lab_pack->total_restocking, lab_pack->shared_objectives[0], lab_pack->shared_objectives[1]);
	return NULL;
}

void *infected(void *thread_pack)
{
	infected_data *inf_pack = thread_pack;
	supply *supply_helper;
	int i, supply_type_needed, supply_val, supply_position_table, inf_id = inf_pack->inf_id;

	// Supplies found helper
	int *supplies_found_position = malloc(sizeof(int) * inf_pack->supplies_type_needed_size); // Store the supply that will be collected by infected
	int supplies_found_index; // Auxiliar Variable to supplies_found_position array

	do
	{
		#if DEBUG_MODE
		printf("> INF %d: Objective Status: Done %d | LAB: %d | INF: %d \n", inf_id, inf_pack->total_vaccines, inf_pack->shared_objectives[0], inf_pack->shared_objectives[1]);
		#endif
		
		// If already completed the objective, wait 10 miliseconds before continue.
		if (inf_pack->total_vaccines > inf_pack->total_vaccines_objective) {
			Sleep(10);
		}

		supplies_found_index = 0;

		// Verify if has supplies without LOCKING TABLE of supplies
		for (i = 0; i < inf_pack->supplies_type_needed_size && supplies_found_index < inf_pack->supplies_type_needed_size; i++) { // Run while has needed types to check AND didnt found the supplies needed 
			supply_type_needed = inf_pack->supplies_type_needed[i]; // Type of Supply that will be searched in the table

			// Iterate on the Table of Supplies
			for (int j = 0; j < inf_pack->size_of_supplies; j++) {
				supply_helper = &inf_pack->supply_table[j]; // Supply fro table that will be verified

				// Compare if the supply in has the same type of the supply needed
				if (supply_type_needed == supply_helper->type)
				{
					sem_getvalue(&supply_helper->semaphore_supply, &supply_val); // Get the quantity of this supply is avaiable

					#if DEBUG_MODE
					printf("> INF %d: Searchs %d | Found: %d | Avaiable %d - SEM_MEM: %p \n", inf_id, supply_type_needed, supply_helper->type, supply_val, supply_helper->semaphore_supply);
					#endif

					// If has one unit avaiable
					if (supply_val == 1)
					{
						supplies_found_position[supplies_found_index++] = j; // Saves the position of the supply to use it later. Add +1 to supplies_found_index

						#if DEBUG_MODE
						printf("> INF %d: Holding Supply Type %d | Total holding: %d -- SEM_MEM: %p \n", inf_id, supply_type_needed, supplies_found_index, supply_helper->semaphore_supply);
						#endif

						break; // If found one supply avaiable, breaks the internal loop and continue trying to find the others
					}
				}
			}
		}

		// In case of not found all supplies needed, continue to the next looping
		if (!(supplies_found_index == inf_pack->supplies_type_needed_size)) {
			continue;
		}

		pthread_mutex_lock(inf_pack->mutex_table); // Start of CRITICAL REGION

		// Double check if its avaiable after locking the table.
		supplies_found_index = 0;
		for (i = 0; i < inf_pack->supplies_type_needed_size; i++) {
			supply_position_table = supplies_found_position[i]; // Gets the position of the first supply that will be checked
			supply_helper = &inf_pack->supply_table[supply_position_table]; // Supply from table that will be verified
			sem_getvalue(&supply_helper->semaphore_supply, &supply_val); // Get the quantity of this supply is avaiable

			if (supply_val == 1) {
				supplies_found_index++;
			}
		}

		// Case didnt the supplies found before locking the table are not avaiable at this point.
		if (!(supplies_found_index == inf_pack->supplies_type_needed_size)) {
			pthread_mutex_unlock(inf_pack->mutex_table); // END OF CRITICAL REGION - mutex_shared_objectives
			continue;
		}
		// Supplies avaiable and ready to consume
		else {
			for (i = 0; i < supplies_found_index; i++) {
				supply_position_table = supplies_found_position[i]; // Gets the position of the first supply that will be checked
				supply_helper = &inf_pack->supply_table[supply_position_table]; // Supply from table that will be verified

				#if DEBUG_MODE
				printf("> INF %d: Consumed Supply of type %d -- SEM_MEM: %p \n", inf_id, supply_helper->type, supply_helper->semaphore_supply);
				#endif

				sem_wait(&supply_helper->semaphore_supply); // Consume the supply
			}

			pthread_mutex_unlock(inf_pack->mutex_table); // End of CRITICAL REGION - Unlock table after consume the supplies

			// Add +1 vacine made to arrive the objective
			inf_pack->total_vaccines++;

			// Verify if this infected completed the objective
			if (inf_pack->total_vaccines == inf_pack->total_vaccines_objective)
			{
				pthread_mutex_lock(inf_pack->mutex_shared_objectives);  // START OF CRITICAL REGION - mutex_shared_objectives
				inf_pack->shared_objectives[1]++; // Add one lab that finished their objective
				pthread_mutex_unlock(inf_pack->mutex_shared_objectives); // END OF CRITICAL REGION - mutex_shared_objectives

				printf("> INF %d: Objective Completed. Objective Completed. Labs Completed: %d | Infected Completed %d \n", inf_id, inf_pack->shared_objectives[0], inf_pack->shared_objectives[1]);
			}
		}

	} while (inf_pack->shared_objectives[0] != inf_pack->size_of_labs || inf_pack->shared_objectives[1] != inf_pack->size_of_infected); // check how many labs and infected ended its work.

	printf("> INF %d: Stopped. Total Vaccined %d \n", inf_id, inf_pack->total_vaccines);
	return NULL;
}

#pragma endregion

/*
*	The idea of this approach is to use an input in a parameterized way to be able to run tests in different ways, in different situations.
*	
*	In the begging of the code is a #define of DEBUG MODE. For tests with 1-100 of objective, its ok to use debugging mode. But to test with bigger numbers I strongly recommend to deactivate it.
*/
int main(int argc, char **argv)
{
	int total_vaccines_objective, size_of_labs, size_of_infected, size_of_supplies, total_supplies_by_lab, total_supplies_needed_infected;

	printf("Started\n");
	/* Instanciation of variables */
	size_of_labs = 3;
	size_of_supplies = 3;
	size_of_infected = 3;
	total_supplies_by_lab = 2;
	total_supplies_needed_infected = 2;
	total_vaccines_objective = atoi(argv[1]);

	// PLEASE TAKE CARE WITH size_of_supplies. This represents that exists 3 differents kinds of supplies
	// But you will find in the code sometimes the operation `size_of_supplies * total_supplies_by_lab`. This means that the table of supplies will have 3*2 elements, in this case.
	// (This was a very hard bug to identify during the development. I was working with only 3 elements and didn`t notice at all).

	/* Instaciation of structs */
	supply *array_supplies = malloc(sizeof(supply) * (size_of_supplies * total_supplies_by_lab)); // In this case, array with 6 semaphores of 3 different types of supply. Supply 1, 2 and 3.
	laboratory_data *array_laboratory = malloc(sizeof(laboratory_data) * size_of_labs); // 3 Labs, each of then produce 2 types of supply. Each supply has your own semaphore.
	infected_data *array_infected = malloc(sizeof(infected_data) * size_of_infected);	// 3 Infected. Each infected needs 2 different types of supply to create the vaccine.

	// Start mutex_table and mutex_shared_objectives
	pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutex_shared_objectives = PTHREAD_MUTEX_INITIALIZER;
	int shared_objectives[2] = { 0, 0 }; // Represents the number of Labs and Infecteds that completed its objective. Position 0 to Labs and 1 to Infecteds.

	// Fill supplies, lab data, infected data
	fill_supplies_data(array_supplies, size_of_supplies, total_supplies_by_lab);
	fill_laboratory_data(array_laboratory, size_of_labs, array_supplies, size_of_supplies, total_supplies_by_lab, total_vaccines_objective, &mutex_table, shared_objectives, &mutex_shared_objectives, size_of_infected);
	fill_infected_data(array_infected, size_of_infected, array_supplies, size_of_supplies, total_supplies_needed_infected, total_vaccines_objective, &mutex_table, shared_objectives, &mutex_shared_objectives, size_of_labs);

	int error_thread;
	for (int i = 0; i < size_of_labs; i++)
	{
		error_thread = pthread_create(&(array_laboratory[i].thread_id), NULL, laboratory, &(array_laboratory[i]));

		if (error_thread != 0)
		{
			printf("Fail to create Lab Thread %d\n, Error code: %d\n", i, error_thread);
			exit(-1);
		}
	}

	for (int i = 0; i < size_of_infected; i++)
	{
		error_thread = pthread_create(&(array_infected[i].thread_id), NULL, infected, &(array_infected[i]));

		if (error_thread != 0)
		{
			printf("Fail to create Inf Thread %d\n, Error code: %d\n", i, error_thread);
			exit(-1);
		}
	}

	for (int i = 0; i < size_of_labs; i++)
	{
		error_thread = pthread_join(array_laboratory[i].thread_id, NULL);

		if (error_thread != 0)
		{
			printf("Fail to receive Thread %d, Error code: %d\n", i, error_thread);
			exit(-2);
		}
	}

	for (int i = 0; i < size_of_infected; i++)
	{
		error_thread = pthread_join(array_infected[i].thread_id, NULL);

		if (error_thread != 0)
		{
			printf("Fail to receive Thread %d, Error code: %d\n", i, error_thread);
			exit(-2);
		}
	}

	free(array_laboratory);
	free(array_infected);
	free(array_supplies);

	printf("End of program.");

	return 0;
}
