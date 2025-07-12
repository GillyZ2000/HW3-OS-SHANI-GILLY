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
    int thread_id = ((int)arg);
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

        pthread_mutex_unlock(&queue->mutex);

        // Process the request
        process_request(request.connfd);
    }
    return NULL;
}

// Opaque struct definition
struct Server_Log {
    // ניהול הבאפר
    char* buffer;              // מערך דינמי לאחסון הלוגים
    size_t size;              // גודל נוכחי של המידע בבאפר
    size_t capacity;          // הקיבולת הנוכחית של הבאפר
    
    // משתני סנכרון
    pthread_mutex_t lock;      
    pthread_cond_t read_cv;    
    pthread_cond_t write_cv;   
    
    // מונים לניהול גישה
    int active_readers;       
    int waiting_writers;      
    int writing;             
};
// Creates a new server log instance (stub)
server_log create_log() {
    struct Server_Log* log = (struct Server_Log*)malloc(sizeof(struct Server_Log));
    if (log == NULL) return NULL;
    
    // נתחיל עם באפר בגודל התחלתי סביר (נגיד 4KB)
    log->capacity = 4096;  // 4KB initial size
    log->buffer = (char*)malloc(log->capacity);
    if (log->buffer == NULL) {
        free(log);
        return NULL;
    }
    log->size = 0;
    
    // אתחול משתני הסנכרון
    pthread_mutex_init(&log->lock, NULL);
    pthread_cond_init(&log->read_cv, NULL);
    pthread_cond_init(&log->write_cv, NULL);
    
    // אתחול המונים
    log->active_readers = 0;
    log->waiting_writers = 0;
    log->writing = 0;
    
    return log;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    if (log == NULL) return;

    // שחרור הבאפר
    if (log->buffer != NULL) {
        free(log->buffer);
    }

    // השמדת מנגנוני הסנכרון
    pthread_mutex_destroy(&log->lock);
    pthread_cond_destroy(&log->read_cv);
    pthread_cond_destroy(&log->write_cv);

    // שחרור המבנה עצמו
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access

    const char* dummy = "Log is not implemented.\n";
    int len = strlen(dummy);
    dst = (char)malloc(len + 1); // Allocate for caller
    if (*dst != NULL) {
        strcpy(*dst, dummy);
    }
    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    writer_lock(&log);
    data_len++;
    // בדיקה אם יש מקום בבאפר
    if (log->size + data_len > log->capacity) {
        // מגדיל את הבאפר
        log->capacity *= 2;
        log->buffer = (char*)realloc(log->buffer, log->capacity);
    }

    // מעתיק את הנתונים לבאפר
    memcpy(log->buffer + log->size, data, data_len);
    log->size += data_len;

    // מעדכן את הבאפר
    log->buffer[log->size] = '\0';

    writer_unlock(&log);
    return;
}
// מאתחל את מנגנון הסנכרון
void readers_writers_init(server_log log) {
    pthread_mutex_init(&log->lock, NULL);
    pthread_cond_init(&log->read_cv, NULL);
    pthread_cond_init(&log->write_cv, NULL);
    log->active_readers = 0;
    log->waiting_writers = 0;
    log->writing = 0;
}

// נעילה לקורא - מחכה אם יש כותב או כותבים ממתינים
void reader_lock(server_log log) {
    pthread_mutex_lock(&log->lock);
    
    // מחכים אם יש כותב פעיל או כותבים ממתינים (העדפת כותבים)
    while (log->writing || log->waiting_writers > 0) {
        pthread_cond_wait(&log->read_cv, &log->lock);
    }
    
    log->active_readers++;
    pthread_mutex_unlock(&log->lock);
}

// שחרור נעילה לקורא
void reader_unlock(server_log log) {
    pthread_mutex_lock(&log->lock);
    
    log->active_readers--;
    
    // אם זה הקורא האחרון והיש כותבים מחכים, נודיע לכותב
    if (log->active_readers == 0 && log->waiting_writers > 0) {
        pthread_cond_signal(&log->write_cv);
    }
    
    pthread_mutex_unlock(&log->lock);
}

// נעילה לכותב - מחכה אם יש קוראים פעילים או כותב אחר
void writer_lock(server_log log) {
    pthread_mutex_lock(&log->lock);
    
    log->waiting_writers++;
    
    // מחכים אם יש קוראים פעילים או כותב אחר
    while (log->active_readers > 0 || log->writing) {
        pthread_cond_wait(&log->write_cv, &log->lock);
    }
    
    log->waiting_writers--;
    log->writing = 1;
    
    pthread_mutex_unlock(&log->lock);
}

// שחרור נעילה לכותב
void writer_unlock(server_log log) {
    pthread_mutex_lock(&log->lock);
    
    log->writing = 0;
    
    // אם יש כותבים מחכים, ניתן להם קדימות
    if (log->waiting_writers > 0) {
        pthread_cond_signal(&log->write_cv);
    }
    // אחרת, נעיר את כל הקוראים
    else {
        pthread_cond_broadcast(&log->read_cv);
    }
    
    pthread_mutex_unlock(&log->lock);
}