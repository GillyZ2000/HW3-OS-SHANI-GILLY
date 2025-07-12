
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include <pthread.h>



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
    *dst = (char*)malloc(len + 1); // Allocate for caller
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

