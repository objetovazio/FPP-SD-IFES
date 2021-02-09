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
	int id;
	int type;
	sem_t semaphore_supply;
} supply;

typedef struct infected_data {
	pthread_t id;
	int thread_number;

	// Semaforos e itens que consome
	supply *supply_table;
	pthread_mutex_t *mutex_table;
	int total_vaccines;

	// TODO: Fail to malloc supplie array dinamically HELP 
	int supplies_needed_type[2];

} infected_data;

typedef struct laboratory_data {
	pthread_t id;
	int thread_number;

	// Semaforo e Mutex dos itens que produz
	supply *supply_table;
	int size_of_supplies;
	pthread_mutex_t *mutex_table;
	int total_restocking;

	// TODO: Fail to malloc supplie array dinamically HELP 
	supply *supplies_production;

	int supplies_production_id[2];
	int total_supplies_by_lab;
} laboratory_data;
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
		array_supplies[i].id   = i;
		array_supplies[i].type = supply_type + 1;

		sem_init(&array_supplies[i].semaphore_supply, 0, 0);

		if (supply_type == total_supplies_by_lab) supply_type = 0;
		else supply_type++;
	}
}

/*
* Fill the laboratories structs, preparing it to start the threads
*/
void fill_laboratory_data(laboratory_data *array_labs, int size_of_labs, supply *array_supplies, int size_of_supplies, int total_supplies_by_lab, pthread_mutex_t *mutex_table)
{
	int supplies_index = 0;
	for (int i = 0; i < size_of_labs; i++) {
		array_labs[i].thread_number = i;
		array_labs[i].total_restocking = 0;
		array_labs[i].mutex_table = mutex_table;
		array_labs[i].supply_table = array_supplies;
		array_labs[i].size_of_supplies = size_of_supplies;
		array_labs[i].total_supplies_by_lab = total_supplies_by_lab;

		// TODO: Fail to malloc supplies array dinamically HELP 
		array_labs[i].supplies_production = malloc(sizeof(supply) * total_supplies_by_lab);

		// Set to supplies_production the supplies that the lab will produce (the semaphores) PS: Attencion to the "++" operator
		int j = 0;
		while (j < total_supplies_by_lab) {
			array_labs[i].supplies_production[j] = array_supplies[supplies_index];
			array_labs[i].supplies_production_id[j] = supplies_index;
			
			int lab_id = array_labs[i].thread_number;
			int supply_id = array_labs[i].supplies_production[j].id;
			int supply_type = array_labs[i].supplies_production[j].type;
			printf("# LAB %d: Semaphore[Id:%d][Type:%d]\n", lab_id, supply_id, supply_type);

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
	int supplies_index = 0;
	for (int i = 0; i < size_of_infected; i++) {
		array_inf[i].thread_number = i;
		array_inf[i].total_vaccines = 0;
		array_inf[i].mutex_table = mutex_table;
		array_inf[i].supply_table = array_supplies;
		array_inf[i].total_vaccines = total_vaccines_objective;

		// TODO: Fail to malloc supplies array dinamically HELP 
		// array_inf[i].supplies_production = malloc(sizeof(supply) * total_supplies_by_lab);

		// Set to supplies_needed_id the supplies ID that the infected have to find on the table
		int j = 0;
		while (j < total_supplies_needed_infected) {
			array_inf[i].supplies_needed_type[j] = supplies_index;

			// Rotates the supplies_index to the max, and then back to 0; 
			if (supplies_index == size_of_supplies - 1) supplies_index = 0;
			else supplies_index++;

			j++;
		}
	}
}
#pragma endregion

void *laboratory(void *thread_pack) {
	laboratory_data *pack = thread_pack;

	int counterTest = 5; // Temporary
	int i, emptySupplies;
	
	int lab_id = pack->thread_number; 

	//if (lab_id == 0 || lab_id == 1) return (void*)pack; // Temporary

	do {
		emptySupplies = TRUE;
		pthread_mutex_lock(pack->mutex_table);

		int supply_id, supply_type, supply_val, supply_old_val;
		// Check if all lab supplies are empty
		for (i = 0; i < pack->total_supplies_by_lab; i++) {
			supply_id   = pack->supplies_production[i].id;
			supply_type = pack->supplies_production[i].type;
			sem_getvalue(&pack->supplies_production[i].semaphore_supply, &supply_val);

			#if DEBUG_MODE
				printf("> LAB %d: Semaphore[Id:%d][Type:%d] has Value %d (i-%d) -- CounterTest: %d\n", lab_id, supply_id, supply_type, supply_val, i, counterTest);
			#endif

			if (supply_val == 1) {
				emptySupplies = FALSE;
				break;
			}
		}

		// If all of them are empty, create new ones
		if (emptySupplies) {
			for (i = 0; i < pack->total_supplies_by_lab; i++) {
				sem_getvalue(&pack->supplies_production[i].semaphore_supply, &supply_old_val);
				sem_post(&pack->supplies_production[i].semaphore_supply);
				sem_getvalue(&pack->supplies_production[i].semaphore_supply, &supply_val);

				#if DEBUG_MODE
					supply_id = pack->supplies_production[i].id;
					supply_type = pack->supplies_production[i].type;
					printf(">> LAB %d: Semaphore[Id:%d][Type:%d] changed value from: %d > to: %d\n", lab_id, supply_id, supply_type, supply_old_val, supply_val);
				#endif // DEBUG_MODE
			}

			#if DEBUG_MODE
				printf(">>> Lab %d is Empty. Supplies refilled.\n", lab_id);
			#endif
		}

		counterTest--;
	} while (counterTest);

	printf(">>>> Lab %d: Ended.\n", lab_id);
}


//void *infected(void *pack) {
//
//}

int main(int argc, char **argv)
{
	int total_vaccines_objective, total_vaccines_objective_prod, size_of_labs, size_of_infected, size_of_supplies, total_supplies_by_lab, total_supplies_needed_infected;

	/* Instanciation of variables */
	size_of_labs                   = 3;
	size_of_supplies               = 3;
	size_of_infected               = 3;
	total_supplies_by_lab          = 2;
	total_vaccines_objective       = 10; // atoi(argv[2]);
	total_supplies_needed_infected = 2;

	/* Instaciation of structs */
	supply *array_supplies            = malloc(sizeof(supply) * size_of_supplies); // In this case, array with 6 semaphores of 3 different types of supply. Supply 1, 2 and 3.
	laboratory_data *array_laboratory = malloc(sizeof(laboratory_data) * size_of_labs); // 3 Labs, each of then produce 2 types of supply. Each supply has your own semaphore.
	infected_data *array_infected     = malloc(sizeof(infected_data) * size_of_infected); // 3 Infected. Each infected needs 2 different types of supply to create the vaccine.

	// Start mutex_table
	pthread_mutex_t *mutex_table;
	pthread_mutex_init(&mutex_table, NULL);

	// Fill supplies, lab data, infected data
	fill_supplies_data(array_supplies, size_of_supplies, total_supplies_by_lab);
	fill_laboratory_data(array_laboratory, size_of_labs, array_supplies, size_of_supplies, total_supplies_by_lab, mutex_table);
	fill_infected_data(array_infected, size_of_infected, array_supplies, size_of_supplies, total_supplies_needed_infected, total_vaccines_objective, mutex_table);

	int error_thread;
	for (int i = 0; i < size_of_labs; i++)
	{
		error_thread = pthread_create(&(array_laboratory[i].id), NULL, laboratory, &(array_laboratory[i]));

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

		if (error_thread != 0)
		{
			printf("Fail to receive Thread %d, Error code: %d\n", i, error_thread);
			exit(-2);
		}
	}

	free(array_laboratory);
	free(array_infected);
	//free(array_supplies);

	printf("Terminado.");

	return 0;
}

// $ gcc -Wall -pedantic -o main.exe main.c -lpthread
