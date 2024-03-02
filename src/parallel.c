#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "../utils/log/log.h"
#include "../utils/utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
pthread_mutex_t graph_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

/* TODO: Define graph task argument. */
typedef struct {
    unsigned int node_index;
} graph_task_arg_t;

static void process_node(unsigned int idx);

void process_node_wrapper(void *arg) {
    
	graph_task_arg_t *task_arg = (graph_task_arg_t *)arg;
    process_node(task_arg->node_index);
	enqueue_task(tp, create_task(&process_node_wrapper, task_arg, free));
}

static void process_node(unsigned int idx)
{
    if (graph->visited[idx] != DONE) {
        os_node_t *node;

        node = graph->nodes[idx];

        pthread_mutex_lock(&sum_mutex);
        sum += node->info;
        pthread_mutex_unlock(&sum_mutex);

        pthread_mutex_lock(&graph_mutex);
        graph->visited[idx] = DONE;
        pthread_mutex_unlock(&graph_mutex);

        for (unsigned int i = 0; i < node->num_neighbours; i++) {
			pthread_mutex_lock(&graph_mutex);
            if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
                graph_task_arg_t *task_arg = malloc(sizeof(*task_arg));
                task_arg->node_index = node->neighbours[i];
                os_task_t *new_task = create_task(&process_node_wrapper, task_arg, free);
            }
			pthread_mutex_unlock(&graph_mutex);
        }
    }
}

int main(int argc, char *argv[]) {
    FILE *input_file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    DIE(input_file == NULL, "fopen");

    graph = create_graph_from_file(input_file);

    /* TODO: Initialize graph synchronization mechanisms. */

    tp = create_threadpool(NUM_THREADS);
    tp->shutdown = 0;
    list_init(&tp->head);

    graph_task_arg_t *task_arg = malloc(sizeof(*task_arg));
    task_arg->node_index = 0;
    os_task_t *new_task = create_task(&process_node_wrapper, task_arg, free);

    // Lock the queue mutex
    pthread_mutex_lock(&queue_mutex);

    // Enqueue the initial task
    enqueue_task(tp, new_task);

    // Unlock the queue mutex
    pthread_mutex_unlock(&queue_mutex);

    // Signal the condition variable to wake up waiting threads
    pthread_cond_signal(&queue_cond);

    wait_for_completion(tp);
    destroy_threadpool(tp);

    printf("%d", sum);

    return 0;
}
