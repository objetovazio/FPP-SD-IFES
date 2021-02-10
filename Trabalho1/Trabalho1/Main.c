#define _CRT_SECURE_NO_WARNINGS 1 
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1 

#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

// Set TRUE or FALSE to show or not Debug Prints
#define DEBUG_MODE TRUE

#pragma region Structs
typedef struct supply {
	int id;					// Supply ID
	int type;				// Supply Type
	sem_t semaphore_supply; // Supply Semaphore to represents if it's avaiable to be collected
} supply;

typedef struct laboratory_data {
	pthread_t id;
	int thread_number;

	// Shared Properties
	pthread_mutex_t *mutex_table;	// Mutex to lock the table of supplies
	supply *supply_table;			// Table of supplies that the lab will lock when verifying it's production
	int size_of_supplies;			// Size of the table of supplies

	// Lab Properties
	supply **supplies_production;	// Array of pointers to which supplies the lab will produce
	int supplies_production_size;		// Total of supplies that lab will produce
	int total_restocking;			// Total full restocking supplies (should count +1 when create new supplies)
	int total_restocking_objective; // Total of restocking that a lab should achieve at least
} laboratory_data;

typedef struct infected_data {
	pthread_t id;
	int thread_number;

	// Shared Properties
	pthread_mutex_t *mutex_table; // Mutex to lock the table of supplies
	supply *supply_table;		  // Table of supplies that the lab will lock when verifying it's production
	int size_of_supplies;		  // Size of the table of supplies

	// Infected Properties
	int *supplies_type_needed;	   // Type of supplies needed by the user
	int supplies_type_needed_size; // Type of supplies needed by the user
	int total_vaccines;		       // Total of times that infected got a vaccine
	int total_vaccines_objective;  // Total of restocking that a lab should achieve at least
} infected_data;

#pragma endregion

#pragma region Fill Structs
/*
* Fill the supplies semaphores, preparing it to start the threads.
* Supplies MUST start with no production (Semaphore starts as 0)
*/
void fill_supplies_data(supply *array_supplies, int size_of_supplies, int total_supplies_by_lab)
{
	int supply_type = 0;

	for (int i = 0; i < size_of_supplies * total_supplies_by_lab; i++) {
		array_supplies[i].id = i;
		array_supplies[i].type = supply_type + 1;

		sem_init(&array_supplies[i].semaphore_supply, 0, 0);

		if (supply_type == total_supplies_by_lab) supply_type = 0;
		else supply_type++;
	}
}

/*
* Fill the laboratories structs, preparing it to start the threads
*/
void fill_laboratory_data(laboratory_data *array_labs, int size_of_labs, supply *array_supplies, int size_of_supplies, int total_supplies_by_lab, int total_restocking_objective, pthread_mutex_t *mutex_table)
{
	int supplies_index = 0;
	for (int i = 0; i < size_of_labs; i++) {
		array_labs[i].thread_number = i;
		array_labs[i].mutex_table = mutex_table;
		array_labs[i].supply_table = array_supplies;
		array_labs[i].size_of_supplies = size_of_supplies;
		array_labs[i].total_restocking = 0;
		array_labs[i].total_restocking_objective = total_restocking_objective;

		// TODO: Fail to malloc supplies array dinamically HELP 
		array_labs[i].supplies_production = malloc(sizeof(supply) * total_supplies_by_lab);
		array_labs[i].supplies_production_size = total_supplies_by_lab;

		// Set to supplies_production the supplies that the lab will produce (the semaphores) PS: Attencion to the "++" operator
		int j = 0;
		while (j < total_supplies_by_lab) {
			array_labs[i].supplies_production[j] = &(array_supplies[supplies_index]); // Set the memory address of a existing supply to a pointer to supply in lab

#if DEBUG_MODE
			int lab_id = array_labs[i].thread_number;
			int supply_id = array_labs[i].supplies_production[j]->id;
			int supply_type = array_labs[i].supplies_production[j]->type;
			printf("# LAB %d: Semaphore[Id:%d][Type:%d] CREATED\n", lab_id, supply_id, supply_type);
#endif
			j++;
			supplies_index++;
		}
	}
}

/*
* Fill the infecteds structs, preparing it to start the threads
*/
void fill_infected_data(infected_data *array_inf, int size_of_infected, supply *array_supplies, int size_of_supplies, int total_supplies_needed_infected, int total_vaccines_objective, pthread_mutex_t *mutex_table)
{
	int supplies_type_index = 0; // Represent the index of supply type. Its used to rotate the Types.
	for (int i = 0; i < size_of_infected; i++) {
		array_inf[i].thread_number = i;
		array_inf[i].mutex_table = mutex_table;
		array_inf[i].supply_table = array_supplies;
		array_inf[i].size_of_supplies = size_of_supplies;
		array_inf[i].total_vaccines = 0;
		array_inf[i].total_vaccines_objective = total_vaccines_objective;

		// TODO: Fail to malloc supplies array dinamically HELP 
		array_inf[i].supplies_type_needed = malloc(sizeof(int) * total_supplies_needed_infected);
		array_inf[i].supplies_type_needed_size = total_supplies_needed_infected;

		// Set to supplies_needed_id the supplies ID that the infected have to find on the table
		int j = 0;
		while (j < total_supplies_needed_infected) {
			array_inf[i].supplies_type_needed[j] = (supplies_type_index + 1);

#if DEBUG_MODE
			int inf_id = array_inf[i].thread_number;
			int supply_id = array_inf[i].supplies_type_needed[j];
			int supply_type = array_inf[i].supplies_type_needed[j];
			printf("# INF %d: Semaphore[Id:%d][Type:%d] CREATED\n", inf_id, supply_id, supply_type);
#endif

			// Rotates the supplies_index to the max, and then back to 0; 
			if (supplies_type_index == size_of_supplies - 1) supplies_type_index = 0;
			else supplies_type_index++;

			j++;
		}
	}
}
#pragma endregion

#pragma region Threads
void *laboratory(void *thread_pack) {
	laboratory_data *pack = thread_pack;

	int counterTest = 5; // Temporary
	int i, emptySupplies;

	int lab_id = pack->thread_number;

	//if (lab_id == 1 || lab_id == 2) return (void*)pack; // Temporary

	do {
		emptySupplies = TRUE;

		pthread_mutex_lock(pack->mutex_table);

		int supply_id, supply_type, supply_val, supply_old_val;
		// Check if all lab supplies are empty
		for (i = 0; i < pack->supplies_production_size; i++) {
			supply_id = pack->supplies_production[i]->id;
			supply_type = pack->supplies_production[i]->type;
			sem_getvalue(&pack->supplies_production[i]->semaphore_supply, &supply_val);

#if DEBUG_MODE
			printf("> LAB %d: Semaphore[Id:%d][Type:%d] has Value %d (i-%d) -- CounterTest: %d\n", lab_id, supply_id, supply_type, supply_val, i, counterTest);
#endif

			if (supply_val == 1) {
				emptySupplies = FALSE;
				pthread_mutex_unlock(pack->mutex_table);
				break;
			}
		}

		// If all of them are empty, create new ones
		if (emptySupplies) {
			for (i = 0; i < pack->supplies_production_size; i++) {
				sem_getvalue(&pack->supplies_production[i]->semaphore_supply, &supply_old_val);
				sem_post(&pack->supplies_production[i]->semaphore_supply);
				sem_getvalue(&pack->supplies_production[i]->semaphore_supply, &supply_val);

#if DEBUG_MODE
				supply_id = pack->supplies_production[i]->id;
				supply_type = pack->supplies_production[i]->type;
				printf(">> LAB %d: Semaphore[Id:%d][Type:%d] changed value from: %d > to: %d\n", lab_id, supply_id, supply_type, supply_old_val, supply_val);
#endif // DEBUG_MODE
			}

#if DEBUG_MODE
			printf(">>> Lab %d is Empty. Supplies refilled.\n", lab_id);
#endif
		}

		pthread_mutex_unlock(pack->mutex_table);
		counterTest--;
	} while (counterTest);

	printf(">>>> Lab %d: Ended.\n", lab_id);
	return NULL;
}

void *infected(void *thread_pack) {
	infected_data *pack = thread_pack;

	int counterTest = 10; // Temporary
	int i, hasAllSupplies;

	int inf_id = pack->thread_number;

	//if (lab_id == 1 || lab_id == 2) return (void*)pack; // Temporary

	supply **supplies_found = malloc(sizeof(supply) * pack->supplies_type_needed_size); // Store the supply that will be collectedby infected

	do {
		hasAllSupplies = FALSE;
		pthread_mutex_lock(pack->mutex_table);

		int supply_id, supply_type_needed, supply_type_looking, supply_val, supply_old_val, supplies_found_index;
		supplies_found_index = 0;
		
		// Check if has both supplies needed avaiable
		for (int i = 0; i < pack->supplies_type_needed_size && supplies_found_index < pack->supplies_type_needed; i++) { // Run while has types to verify and didnt found all necessary.
			#if DEBUG_MODE
			supply_type_needed = pack->supplies_type_needed[i];
			#endif
			for (int j = 0; j < pack->size_of_supplies; j++) {

				#if DEBUG_MODE
				supply_type_looking = pack->supply_table[j].type;
				printf("> INF %d: Looking for Type %d. Actual Type: %d -- CounterTest: %d\n", inf_id, supply_type_needed, supply_type_looking, counterTest);
				#endif

				// Compare if the supply in the table is the supply needed type
				if (pack->supplies_type_needed[i] == pack->supply_table[j].type) {
					sem_getvalue(&pack->supply_table[j].semaphore_supply, &supply_val);

					#if DEBUG_MODE
					supply_type_needed = pack->supplies_type_needed[i];
					supply_type_looking = pack->supply_table[j].type;
					printf("> INF %d: Looking for Type %d. Actual Type: %d -- AVAIABLE: %d CounterTest: %d\n", inf_id, supply_type_needed, supply_type_looking, supply_val, counterTest);
					#endif
					
					if (supply_val == 1) {
						supplies_found[supplies_found_index++] = &pack->supply_table[j];
						printf("> INF %d: Saved semaphore for type %d. Breaking loop\n", inf_id, supply_type_looking, supply_val, counterTest);
						break; // If found one, break the internal loop and continue trying to find the rest
					}
				}
			}

			if (supplies_found_index == 0) \
			{
				printf("> INF %d: Not found supply %d, breaking search.\n", inf_id, supply_type_needed, counterTest);
				break;
			} // if didnt found at least one, theres no why to continue the loop;
		}

		pthread_mutex_unlock(pack->mutex_table);
		counterTest--;
	} while (counterTest);

	printf(">>>> INF %d: Ended.\n", inf_id);
	return NULL;
}

#pragma endregion

int main(int argc, char **argv)
{
	int total_vaccines_objective, size_of_labs, size_of_infected, size_of_supplies, total_supplies_by_lab, total_supplies_needed_infected;

	/* Instanciation of variables */
	size_of_labs = 3;
	size_of_supplies = 3;
	size_of_infected = 3;
	total_supplies_by_lab = 2;
	total_vaccines_objective = 10; // atoi(argv[2]);
	total_supplies_needed_infected = 2;

	/* Instaciation of structs */
	supply *array_supplies = malloc(sizeof(supply) * size_of_supplies); // In this case, array with 6 semaphores of 3 different types of supply. Supply 1, 2 and 3.
	laboratory_data *array_laboratory = malloc(sizeof(laboratory_data) * size_of_labs); // 3 Labs, each of then produce 2 types of supply. Each supply has your own semaphore.
	infected_data *array_infected = malloc(sizeof(infected_data) * size_of_infected); // 3 Infected. Each infected needs 2 different types of supply to create the vaccine.

	// Start mutex_table
	pthread_mutex_t mutex_table;
	pthread_mutex_init(&mutex_table, NULL);

	// Fill supplies, lab data, infected data
	fill_supplies_data(array_supplies, size_of_supplies, total_supplies_by_lab);
	fill_laboratory_data(array_laboratory, size_of_labs, array_supplies, size_of_supplies, total_supplies_by_lab, total_vaccines_objective, &mutex_table);
	fill_infected_data(array_infected, size_of_infected, array_supplies, size_of_supplies, total_supplies_needed_infected, total_vaccines_objective, &mutex_table);

	int error_thread;
	for (int i = 0; i < size_of_labs; i++)
	{
		error_thread = pthread_create(&(array_laboratory[i].id), NULL, laboratory, &(array_laboratory[i]));
		error_thread = pthread_create(&(array_infected[i].id), NULL, infected, &(array_infected[i]));

		if (error_thread != 0)
		{
			printf("Fail to create Thread %d\n, Error code: %d\n", i, error_thread);
			exit(-1);
		}

		printf("### Lab %d created.\n", i);
	}

	for (int i = 0; i < size_of_labs; i++)
	{
		error_thread = pthread_join(array_laboratory[i].id, NULL);
		error_thread = pthread_join(array_infected[i].id, NULL);

		if (error_thread != 0)
		{
			printf("Fail to receive Thread %d, Error code: %d\n", i, error_thread);
			exit(-2);
		}
	}

	free(array_laboratory);
	free(array_infected);

	/*for (int i = 0; i < size_of_supplies * total_supplies_by_lab; i++) {
		sem_destroy(&(array_supplies[i].semaphore_supply));
	}*/

	//free(array_supplies);

	printf("Terminado.");

	return 0;
}

// $ gcc -Wall -pedantic -o main.exe main.c -lpthread
