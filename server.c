
#include "segel.h"
#include "request.h"
#include "log.h"

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Structure for request statistics
typedef struct stats {
    struct timeval arrival;    // Stat-req-arrival: arrival time
    struct timeval dispatch;   // For calculating dispatch interval
} stats_t;

// Structure for a request in the queue
typedef struct request_t {
    int connfd;               // Connection file descriptor
    stats_t stats;           // Request timing statistics
} request_t;

// Structure for the request queue
typedef struct request_queue {
    request_t* requests;      // Array of requests
    int queue_size;          // Maximum size from command line argument
    int count;               // Current number of requests in queue
    int front;               // Index for the first request
    int rear;               // Index for the last request
    pthread_mutex_t mutex;   // Mutex for queue synchronization
    pthread_cond_t not_empty;// Condition for queue not empty
    pthread_cond_t not_full; // Condition for queue not full
} request_queue;

// Global variables
request_queue* queue;        // The request queue
pthread_t* threads;         // Array of worker threads
server_log srv_log;

// Parses command-line arguments
void getargs(int *port, int *threads, int *queue_size, int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <threads> <queue_size>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    
    // Validate arguments
    if (*threads <= 0 || *queue_size <= 0) {
        fprintf(stderr, "threads and queue_size must be positive integers\n");
        exit(1);
    }
}
// TODO: HW3 — Initialize thread pool and request queue
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.

// Initialize the request queue
request_queue* init_queue(int queue_size) {
    request_queue* queue = (request_queue*)malloc(sizeof(request_queue));
    if (queue == NULL) {
        fprintf(stderr, "Failed to allocate queue\n");
        exit(1);
    }

    queue->requests = (request_t*)malloc(queue_size * sizeof(request_t));
    if (queue->requests == NULL) {
        fprintf(stderr, "Failed to allocate requests array\n");
        free(queue);
        exit(1);
    }

    queue->queue_size = queue_size;
    queue->count = 0;
    queue->front = 0;
    queue->rear = 0;

    // Initialize synchronization primitives
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return queue;
}

// Initialize the thread pool
void init_thread_pool(int num_threads) {
    threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        fprintf(stderr, "Failed to allocate thread pool\n");
        exit(1);
    }
    num_threads = num_threads;  // Set global variable
}

void* worker_thread(void* arg) {
    int thread_id = *((int*)arg);
    free(arg);

    while (1) {
        pthread_mutex_lock(&queue->mutex);

        // Wait while queue is empty
        while (queue->count == 0) {
            pthread_cond_wait(&queue->not_empty, &queue->mutex);
        }

        // Get the request from the front of the queue
        request_t request = queue->requests[queue->front];
        queue->front = (queue->front + 1) % queue->queue_size;
        queue->count--;

        // Signal that the queue is not full anymore
        pthread_cond_signal(&queue->not_full);
        
        // If there are still items in queue, wake up another worker
        if (queue->count > 0) {
            pthread_cond_broadcast(&queue->not_empty);
        }

        // Get current time for dispatch statistics
        struct timeval dispatch;
        gettimeofday(&dispatch, NULL);

        pthread_mutex_unlock(&queue->mutex);

        // Handle request...
        threads_stats t = malloc(sizeof(struct Threads_stats));
        t->id = thread_id;
        t->total_req = 0;
        t->stat_req = 0;
        t->dynm_req = 0;

        requestHandle(request.connfd, request.stats.arrival, dispatch, t, srv_log);

        Close(request.connfd);
        free(t);
    }
    return NULL;
}

// Cleanup function for the request queue
void destroy_queue(request_queue* queue) {
    if (queue) {
        // Destroy synchronization primitives
        pthread_mutex_destroy(&queue->mutex);
        pthread_cond_destroy(&queue->not_empty);
        pthread_cond_destroy(&queue->not_full);

        // Free the requests array
        if (queue->requests) {
            free(queue->requests);
        }

        // Free the queue structure itself
        free(queue);
    }
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    int num_threads, queue_size;
    struct sockaddr_in clientaddr;

    // Create the global server log
    server_log log = create_log();

    // Parse arguments
    getargs(&port, &num_threads, &queue_size, argc, argv);

    // Initialize the request queue and thread pool
    queue = init_queue(queue_size);
    init_thread_pool(num_threads);

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        int* thread_id = malloc(sizeof(int));
        *thread_id = i;  
        pthread_create(&threads[i], NULL, worker_thread, thread_id);
    }

    listenfd = Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        // Get arrival time
        struct timeval arrival;
        gettimeofday(&arrival, NULL);

        // Lock the queue
        pthread_mutex_lock(&queue->mutex);

        // Wait if queue is full
        while (queue->count >= queue->queue_size) {
            pthread_cond_wait(&queue->not_full, &queue->mutex);
        }

        // Add request to queue
        request_t new_request;
        new_request.connfd = connfd;
        new_request.stats.arrival = arrival;
        
        queue->requests[queue->rear] = new_request;
        queue->rear = (queue->rear + 1) % queue->queue_size;
        queue->count++;

        // Signal that queue is not empty
        pthread_cond_signal(&queue->not_empty);
        
        pthread_mutex_unlock(&queue->mutex);
    }

    // Cleanup code
    for (int i = 0; i < num_threads; i++) {
        pthread_cancel(threads[i]);  // Cancel all worker threads
        pthread_join(threads[i], NULL);  // Wait for them to finish
    }

    // Free thread pool array
    free(threads);

    // Destroy the queue
    destroy_queue(queue);

    // Clean up the server log
    destroy_log(log);

    return 0;
}

