#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct thread_data
{
    pthread_t id;
    unsigned long long thread_number;
    unsigned long long start;
    unsigned long long end;
    unsigned long long *vector;
} thread_data;

int is = 0;
void fill_vector(unsigned long long *vector, long long size_vector)
{
    for (int i = 0; i < size_vector; i++)
    {
        vector[i] = rand() % 100000;
    }
}

void *thread_summation(void *thread_pack)
{
    thread_data *pack = thread_pack;
    unsigned long long sum_thread = 0;

    // printf("Thread %lld is processing from %lld to %lld \n", pack->thread_number, pack->start, pack->end);
    for (unsigned long long i = pack->start; i < pack->end; i++)
        sum_thread += pack->vector[i];

    // printf("Thread %lld finished\n", pack->thread_number);
    return (void *) sum_thread;
}

void calculate_serial(long long size_vector, unsigned long long *vector)
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
}

int calculate_thread(long long size_vector, unsigned long long *vector, int number_threads)
{
    int error_thread = 0;
    unsigned long long i, partial_result, sum_vector = 0, block_size;
    clock_t time;

    /* Thread */
    sum_vector = 0;
    error_thread = 0;
    block_size = size_vector / number_threads;
    thread_data *thread_vector = malloc(sizeof(thread_data) * number_threads);

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

        // printf("The thread %lld will process from %lld to %lld\n", thread_vector[i].thread_number, thread_vector[i].start, thread_vector[i].end);
    }

    time = clock();
    for (i = 0; i < number_threads; i++)
    {
        error_thread = pthread_create(&(thread_vector[i].id), NULL, thread_summation, &(thread_vector[i]));

        if (error_thread != 0)
        {
            // printf("Fail to create Thread %lld\n", i);
            return (-1 * i);
        }
    }

    for (i = 0; i < number_threads; i++)
    {
        error_thread = pthread_join(thread_vector[i].id, (void *) &partial_result);

        if (error_thread != 0)
        {
            // printf("Fail to receive Thread %lld\n", i);
            return (-2 * i);
        }
        
        sum_vector += partial_result;

        // printf("Thread %lld closed\n", i);
    }
    time = clock() - time;

    printf("The sum of %lld elements in Multithread Mode took %ld clicks\nFinal result: %lld\n", size_vector, time, sum_vector);

    return 0;
}

int main(int argc, char **argv)
{
    int number_threads;
    long long size_vector;
    char *eptr;
    /* */
    size_vector = strtoull(argv[1], &eptr, 10);
    number_threads = atoll(argv[2]);
    unsigned long long *vector = malloc(sizeof(unsigned long long) * size_vector);

    printf("### Details\n- Threads: %d\n- Vector Size: %lld\n\n", number_threads, size_vector);

    fill_vector(vector, size_vector);

    printf("### Starting Serial Mode\n");
    calculate_serial(size_vector, vector);

    printf("\n### Starting Multithread Mode\n");
    calculate_thread(size_vector, vector, number_threads);

    free(vector);

    return 0;
}
