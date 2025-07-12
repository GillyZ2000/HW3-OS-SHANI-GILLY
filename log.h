
2 of 794
Fwd: hw3 1
Inbox

koren shani <fuog56@gmail.com>
Attachments
2:46 PM (3 hours ago)
to me



---------- Forwarded message ---------
מאת: koren shani <fuog56@gmail.com>
‪Date: שבת, 12 ביולי 2025 ב-14:44‬
Subject: hw3 1
To: koren shani <fuog56@gmail.com>




 21 Attachments
  •  Scanned by Gmail
#ifndef SERVER_LOG_H
#define SERVER_LOG_H


// TODO:
// Implement a thread-safe server log system.
// - The log should support concurrent access from multiple threads.
// - You must implement a multiple-readers/single-writer synchronization model.
// - Writers must have priority over readers.
//   This means that if a writer is waiting, new readers should be blocked until the writer is done.
// - Use appropriate synchronization primitives (e.g., pthread mutexes and condition variables).
// - The log should allow appending entries and returning the full log content.

typedef struct Server_Log* server_log;


// Creates a new server log instance
server_log create_log();

// Destroys and frees the log
void destroy_log(server_log log);

// Returns the log contents as a string (null-terminated)
// NOTE: caller is responsible for freeing dst
int get_log(server_log log, char** dst);

// Appends a new entry to the log
void add_to_log(server_log log, const char* data, int data_len);

#endif // SERVER_LOG_H
