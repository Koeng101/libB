/*
 * HTTP Server with Lua Evaluation
 * ------------------------------
 * A multi-threaded HTTP server that evaluates Lua code received via POST requests.
 * Uses a thread pool with a connection queue to handle concurrent requests efficiently.
 *
 * Core Architecture:
 * - Main thread: Accepts connections and feeds them to a connection queue
 * - Worker threads: Pull from queue and process requests
 * - Each request creates a new Lua state, evaluates code, returns result
 *
 * Alternative Designs Considered:
 * 1. Process pool instead of thread pool
 *    - Would provide better isolation
 *    - Rejected due to higher resource overhead and slower IPC
 * 2. Single-threaded event loop (like Node.js)
 *    - Would be simpler to reason about
 *    - Rejected because Lua evaluation would block the event loop
 * 3. Thread-per-connection
 *    - Simplest to implement
 *    - Rejected due to resource scaling issues with many connections
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "lua_eval.h"

/*
 * Buffer Size Constants
 * --------------------
 * BUFFER_SIZE: 4KB matches common page size and typical HTTP request size
 * MAX_RESPONSE_SIZE: 1KB balances between typical Lua output and memory usage
 * NUM_WORKERS: Matches typical CPU core count for balanced parallelism
 *
 * Alternative sizes considered:
 * - Larger BUFFER_SIZE (8KB+): Would waste memory for typical requests
 * - Smaller BUFFER_SIZE (1KB): Would require multiple reads for many requests
 * - Dynamic sizing: Rejected due to added complexity in memory management
 */
#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_RESPONSE_SIZE 1024
#define NUM_WORKERS 4

/*
 * Connection Queue
 * ---------------
 * Thread-safe circular buffer for managing client connections.
 * Uses mutex + condition variables for synchronization.
 *
 * Design Decisions:
 * - Circular buffer chosen over linked list for cache efficiency
 * - Fixed size queue provides backpressure under high load
 * - Two condition variables allow precise thread wakeups
 *
 * Alternative Approaches:
 * - Lock-free queue: Rejected due to complexity and limited benefit
 * - Unlimited queue: Rejected to prevent resource exhaustion
 */
typedef struct {
    int* clients;          // Array of client file descriptors
    int head;             // Read position
    int tail;             // Write position
    int size;             // Maximum queue size
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ConnectionQueue;

// Global queue instance
ConnectionQueue queue;

/*
 * Queue Management Functions
 * ------------------------
 * Thread-safe operations for connection queue.
 * Uses standard producer-consumer pattern with condition variables.
 *
 * Error Handling Strategy:
 * - Queue operations cannot fail once initialized
 * - Blocking operations can be interrupted by signals
 */

void init_queue(int size) {
    queue.clients = malloc(size * sizeof(int));
    queue.size = size;
    queue.head = queue.tail = 0;
    pthread_mutex_init(&queue.mutex, NULL);
    pthread_cond_init(&queue.not_empty, NULL);
    pthread_cond_init(&queue.not_full, NULL);
}

/*
 * Queue Operations
 * ---------------
 * Thread-safe enqueue and dequeue operations.
 * Both operations block when queue is full/empty respectively.
 *
 * Design Decisions:
 * - Blocking operations provide natural backpressure
 * - Mutex held only during actual queue manipulation
 * - Condition variables used instead of spinning for CPU efficiency
 *
 * Alternative Approaches:
 * - Non-blocking operations with error returns: Rejected as it would complicate client code
 * - Timeout-based operations: Rejected as timeouts would be arbitrary
 */

void enqueue_client(int client_fd) {
    pthread_mutex_lock(&queue.mutex);
    
    // Wait while queue is full
    while ((queue.tail + 1) % queue.size == queue.head) {
        pthread_cond_wait(&queue.not_full, &queue.mutex);
    }
    
    queue.clients[queue.tail] = client_fd;
    queue.tail = (queue.tail + 1) % queue.size;
    
    pthread_cond_signal(&queue.not_empty);
    pthread_mutex_unlock(&queue.mutex);
}

int dequeue_client() {
    pthread_mutex_lock(&queue.mutex);
    
    // Wait while queue is empty
    while (queue.head == queue.tail) {
        pthread_cond_wait(&queue.not_empty, &queue.mutex);
    }
    
    int client_fd = queue.clients[queue.head];
    queue.head = (queue.head + 1) % queue.size;
    
    pthread_cond_signal(&queue.not_full);
    pthread_mutex_unlock(&queue.mutex);
    return client_fd;
}

/*
 * HTTP Request Handling
 * -------------------
 * Basic HTTP parser and response formatter.
 * Focuses only on POST requests and plain text responses.
 *
 * Design Decisions:
 * - Simple parsing instead of full HTTP parser
 * - Fixed response format optimized for Lua output
 * - Minimal HTTP headers for reduced overhead
 *
 * Alternative Approaches:
 * - Full HTTP parser library: Rejected as overkill for simple needs
 * - Custom protocol: Rejected for compatibility/debugging reasons
 */

char* get_post_body(char* request) {
    char* body = strstr(request, "\r\n\r\n");
    return body ? body + 4 : NULL;
}

void send_response(int client_fd, const char* content) {
    char response[MAX_RESPONSE_SIZE + 256];  // Extra space for headers
    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", strlen(content), content);
    write(client_fd, response, strlen(response));
}

/*
 * Network Setup and Worker Threads
 * ------------------------------
 * Core server infrastructure handling connections and request processing.
 *
 * Design Decisions:
 * - Standard TCP socket setup with common defaults
 * - Worker threads handle complete request lifecycle
 * - One-shot connections (no keep-alive)
 *
 * Alternative Approaches:
 * - Connection pooling: Rejected for simplicity
 * - Async I/O: Rejected as blocking I/O sufficient for this use case
 */

int setup_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE] = {0};
    read(client_fd, buffer, BUFFER_SIZE);

    if (strncmp(buffer, "POST", 4) == 0) {
        char* body = get_post_body(buffer);
        if (body) {
            char* result = lua_eval_string(body);
            send_response(client_fd, result);
        } else {
            send_response(client_fd, "Error: No body in POST request");
        }
    } else {
        send_response(client_fd, "Error: Only POST method is supported");
    }

    close(client_fd);
}

void* worker_thread(void* arg) {
    (void)arg;  // Explicitly mark arg as unused
    while (1) {
        int client_fd = dequeue_client();
        handle_client(client_fd);
    }
    return NULL;
}

/*
 * Main Server Loop
 * --------------
 * Initializes server and manages its lifecycle.
 * Sets up worker threads and handles incoming connections.
 *
 * Design Decisions:
 * - Fixed number of worker threads
 * - Infinite accept loop with error handling
 * - No graceful shutdown (terminated by signal)
 *
 * Alternative Approaches:
 * - Dynamic thread pool: Rejected for simplicity
 * - Event-driven accept: Rejected as unnecessary for scale
 */

int main() {
    int server_fd = setup_server(PORT);
    if (server_fd < 0) return 1;

    init_queue(100);  // Queue size balances memory use vs burst capacity

    pthread_t workers[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    printf("Server listening on port %d with %d workers...\n", PORT, NUM_WORKERS);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }
        enqueue_client(client_fd);
    }

    // Cleanup code never reached in this simple version
    close(server_fd);
    return 0;
}
