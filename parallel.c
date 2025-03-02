// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log/log.h"
#include "os_graph.h"
#include "os_threadpool.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
static pthread_mutex_t mutex_add;
static pthread_mutex_t mutex_grp;

/* TODO: Define graph task argument. */
typedef struct {
	unsigned int node;
} GraphTaskArgs;

static void process_node(unsigned int idx);

static void process_node_task(void *arg)
{
	GraphTaskArgs *task_args = (GraphTaskArgs *)arg;
	unsigned int node = task_args->node;

	if (node < graph->num_nodes) {
		if (graph->visited[node] == NOT_VISITED)
			process_node(node);
	}
}

static void process_node(unsigned int idx)
{
	pthread_mutex_lock(&mutex_grp);
	if (graph->visited[idx] == NOT_VISITED) {
		graph->visited[idx] = DONE;
		pthread_mutex_unlock(&mutex_grp);
		pthread_mutex_lock(&mutex_add);
		os_node_t *node = graph->nodes[idx];

		sum += node->info;
		pthread_mutex_unlock(&mutex_add);

		if (node->num_neighbours > 0) {
			for (unsigned int i = 0; i < node->num_neighbours; i++) {
				unsigned int neighbor = node->neighbours[i];

				pthread_mutex_lock(&mutex_grp);
				if (graph->visited[neighbor] == NOT_VISITED) {
					GraphTaskArgs *task = malloc(sizeof(GraphTaskArgs));

					task->node = neighbor;
					os_task_t *t = create_task(process_node_task, task, free);

					enqueue_task(tp, t);
				}
				pthread_mutex_unlock(&mutex_grp);
			}
		}
	} else {
		pthread_mutex_unlock(&mutex_grp);
	}
}

int main(int argc, char *argv[])
{
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
	GraphTaskArgs *task = malloc(sizeof(GraphTaskArgs));

	task->node = 0;
	os_task_t *first = create_task(process_node_task, task, free);

	enqueue_task(tp, first);
	wait_for_completion(tp);

	printf("%d", sum);
	destroy_threadpool(tp);

	return 0;
}
