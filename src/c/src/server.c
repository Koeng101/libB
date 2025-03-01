/*
 * HTTP Server with Lua Sandbox Evaluation
 * ---------------------------------------
 * A multi-threaded HTTP server that evaluates Lua code received via POST requests
 * with configurable sandbox settings for memory limits and CPU timeout.
 *
 * Core Architecture:
 * - Uses picohttpparser for lightweight HTTP request handling
 * - Processes JSON-formatted requests containing Lua code
 * - Executes code in a sandboxed Lua environment
 * - Returns results as JSON responses
 *
 * Security Considerations:
 * - Memory limits prevent resource exhaustion
 * - CPU timeout prevents infinite loops
 * - JSON parsing validates input structure
 *
 * Alternative Designs Considered:
 * 1. Custom HTTP parser
 *    - Lighter weight but more error-prone
 *    - Rejected due to complexity and potential security issues
 * 2. Single-threaded event loop
 *    - Would be simpler but would block during Lua execution
 *    - Rejected because Lua evaluation would block the event loop
 * 3. Third-party HTTP frameworks
 *    - More features but larger dependencies
 *    - picohttpparser chosen for balance of performance and size
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Include picohttpparser */
#include "picohttpparser.h"

/* Include Lua evaluation and JSON parsing */
#include "lua_eval.h"
#include "cJSON.h"

/*
 * Configuration Constants
 * ----------------------
 * PORT: Default HTTP port (can be overridden)
 * NUM_THREADS: Number of server threads to handle requests
 * REQUEST_TIMEOUT_MS: Timeout for client connections
 * BUFFER_SIZE: Size of read buffer for HTTP requests
 */
#define PORT 8080
#define NUM_THREADS 4
#define REQUEST_TIMEOUT_MS 30000
#define BUFFER_SIZE 8192
#define MAX_HEADERS 100

/*
 * Global Context
 * -------------
 * Stores server context and runtime settings
 * Used across the application
 */
struct server_context {
    int server_fd;
    bool running;
    pthread_t threads[NUM_THREADS];
};

/* Thread argument structure */
struct thread_arg {
    struct server_context *server;
    int thread_id;
};

/*
 * JSON Response Generation
 * ----------------------
 * Helper functions to create consistent JSON responses
 * for both successful results and error conditions
 */

/* 
 * Create a JSON error response
 * Returns a newly allocated string that must be freed
 */
static char *create_error_json(const char *error_message) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "error", error_message);
    char *json_str = cJSON_Print(response);
    cJSON_Delete(response);
    return json_str;
}

/* 
 * Create a JSON response from Lua evaluation result
 * Returns a newly allocated string that must be freed
 */
static char *create_result_json(lua_eval_result result) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "output", result.output);
    cJSON_AddStringToObject(response, "debug", result.debug_output);
    cJSON_AddNumberToObject(response, "status", result.status);
    char *json_str = cJSON_Print(response);
    cJSON_Delete(response);
    return json_str;
}

/*
 * HTTP Request Processing
 * ----------------------
 * Functions to parse and respond to HTTP requests using picohttpparser
 */

/* Send HTTP response */
static void send_response(int client_fd, int status_code, const char *status_text, 
                        const char *content_type, const char *body) {
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, BUFFER_SIZE,
                             "HTTP/1.1 %d %s\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %zu\r\n"
                             "\r\n",
                             status_code, status_text,
                             content_type, strlen(body));
    
    /* Send header */
    send(client_fd, header, header_len, 0);
    
    /* Send body */
    send(client_fd, body, strlen(body), 0);
}

/* Process a Lua evaluation request */
static void process_lua_request(int client_fd, const char *body, size_t body_len) {
    /* Ensure null-terminated body for cJSON */
    char *body_copy = malloc(body_len + 1);
    if (!body_copy) {
        char *error = create_error_json("Server error: Out of memory");
        send_response(client_fd, 500, "Internal Server Error", "application/json", error);
        free(error);
        return;
    }
    
    memcpy(body_copy, body, body_len);
    body_copy[body_len] = '\0';
    
    /* Parse JSON request */
    cJSON *request_json = cJSON_Parse(body_copy);
    free(body_copy);
    
    if (!request_json) {
        char *error = create_error_json("Invalid JSON in request body");
        send_response(client_fd, 400, "Bad Request", "application/json", error);
        free(error);
        return;
    }
    
    /* Extract Lua code */
    cJSON *code_json = cJSON_GetObjectItem(request_json, "code");
    if (!code_json || !cJSON_IsString(code_json)) {
        cJSON_Delete(request_json);
        char *error = create_error_json("Missing or invalid 'code' field");
        send_response(client_fd, 400, "Bad Request", "application/json", error);
        free(error);
        return;
    }
    
    /* Create sandbox configuration */
    lua_sandbox_config config = lua_default_config();
    
    /* Override with custom config if provided */
    cJSON *config_json = cJSON_GetObjectItem(request_json, "config");
    if (config_json && cJSON_IsObject(config_json)) {
        cJSON *memory_limit_json = cJSON_GetObjectItem(config_json, "memory_limit");
        if (memory_limit_json && cJSON_IsNumber(memory_limit_json)) {
            config.memory_limit = (size_t)memory_limit_json->valuedouble;
        }
        
        cJSON *cpu_timeout_json = cJSON_GetObjectItem(config_json, "cpu_timeout");
        if (cpu_timeout_json && cJSON_IsNumber(cpu_timeout_json)) {
            config.cpu_timeout = cpu_timeout_json->valueint;
        }
    }
    
    /* Execute Lua code in sandbox */
    lua_eval_result result = lua_eval_string_sandbox(code_json->valuestring, config);
    
    /* No longer need the request JSON */
    cJSON_Delete(request_json);
    
    /* Create response JSON */
    char *response_json = create_result_json(result);
    
    /* Send response */
    send_response(client_fd, 200, "OK", "application/json", response_json);
    
    /* Cleanup */
    free(response_json);
    lua_free_result(result);
}

/* Handle HTTP connection with picohttpparser */
static void handle_connection(int client_fd) {
    char buf[BUFFER_SIZE];
    ssize_t bytes_read;
    
    /* Read request data */
    bytes_read = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    
    /* Variables for picohttpparser */
    const char *method, *path;
    size_t method_len, path_len;
    int minor_version;
    struct phr_header headers[MAX_HEADERS];
    size_t num_headers = MAX_HEADERS;
    
    /* Parse request */
    int pret = phr_parse_request(buf, bytes_read, &method, &method_len, &path, &path_len,
                               &minor_version, headers, &num_headers, 0);
    
    if (pret <= 0) {
        char *error = create_error_json("Invalid HTTP request");
        send_response(client_fd, 400, "Bad Request", "application/json", error);
        free(error);
        close(client_fd);
        return;
    }
    
    /* Check if it's a POST request */
    if (method_len != 4 || strncmp(method, "POST", 4) != 0) {
        char *error = create_error_json("Only POST method is supported");
        send_response(client_fd, 405, "Method Not Allowed", "application/json", error);
        free(error);
        close(client_fd);
        return;
    }
    
    /* Find Content-Length header */
    long content_length = 0;
    for (size_t i = 0; i < num_headers; i++) {
        if (headers[i].name_len == 14 && strncasecmp(headers[i].name, "Content-Length", 14) == 0) {
            content_length = atol(headers[i].value);
            break;
        }
    }
    
    if (content_length <= 0 || content_length > 1024*1024) { /* 1MB limit */
        char *error = create_error_json("Invalid or missing Content-Length");
        send_response(client_fd, 411, "Length Required", "application/json", error);
        free(error);
        close(client_fd);
        return;
    }
    
    /* Calculate body position */
    size_t header_size = pret;
    const char *body = buf + header_size;
    size_t body_len = bytes_read - header_size;
    
    /* Check if we have the complete body */
    if (body_len < (size_t)content_length) {
        /* If not, read more data */
        size_t remaining = content_length - body_len;
        
        /* If remaining data is too large for our buffer, reject */
        if (header_size + body_len + remaining > BUFFER_SIZE) {
            char *error = create_error_json("Request body too large");
            send_response(client_fd, 413, "Payload Too Large", "application/json", error);
            free(error);
            close(client_fd);
            return;
        }
        
        /* Read remaining data */
        ssize_t more_bytes = recv(client_fd, buf + bytes_read, remaining, 0);
        if (more_bytes <= 0 || (size_t)more_bytes < remaining) {
            char *error = create_error_json("Failed to read complete request body");
            send_response(client_fd, 400, "Bad Request", "application/json", error);
            free(error);
            close(client_fd);
            return;
        }
        
        bytes_read += more_bytes;
        body_len += more_bytes;
    }
    
    /* Process the request */
    process_lua_request(client_fd, body, content_length);
    
    /* Close connection */
    close(client_fd);
}

/*
 * Thread function for handling connections
 */
static void *server_thread(void *arg) {
    struct thread_arg *thread_arg = (struct thread_arg *)arg;
    struct server_context *server = thread_arg->server;
    
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        /* Accept connection */
        int client_fd = accept(server->server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }
        
        /* Handle connection */
        handle_connection(client_fd);
    }
    
    free(arg);
    return NULL;
}

/*
 * Server Configuration and Management
 * ---------------------------------
 * Functions to start, configure, and stop the HTTP server
 */

/*
 * Initialize and start the HTTP server
 * Returns server context or NULL on error
 */
static struct server_context *start_server(int port) {
    struct server_context *ctx = malloc(sizeof(struct server_context));
    if (!ctx) {
        return NULL;
    }
    
    /* Initialize server context */
    ctx->running = false;
    
    /* Create server socket */
    ctx->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->server_fd < 0) {
        free(ctx);
        return NULL;
    }
    
    /* Set socket options */
    int opt = 1;
    if (setsockopt(ctx->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(ctx->server_fd);
        free(ctx);
        return NULL;
    }
    
    /* Bind to port */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(ctx->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ctx->server_fd);
        free(ctx);
        return NULL;
    }
    
    /* Listen for connections */
    if (listen(ctx->server_fd, 10) < 0) {
        close(ctx->server_fd);
        free(ctx);
        return NULL;
    }
    
    /* Mark as running */
    ctx->running = true;
    
    /* Start worker threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        struct thread_arg *arg = malloc(sizeof(struct thread_arg));
        if (!arg) {
            ctx->running = false;
            close(ctx->server_fd);
            free(ctx);
            return NULL;
        }
        
        arg->server = ctx;
        arg->thread_id = i;
        
        if (pthread_create(&ctx->threads[i], NULL, server_thread, arg) != 0) {
            free(arg);
            ctx->running = false;
            close(ctx->server_fd);
            free(ctx);
            return NULL;
        }
    }
    
    printf("Lua Sandbox Server started on port %d with %d threads\n", port, NUM_THREADS);
    
    return ctx;
}

/*
 * Stop the HTTP server and free resources
 */
static void stop_server(struct server_context *ctx) {
    if (!ctx) {
        return;
    }
    
    /* Stop threads */
    ctx->running = false;
    
    /* Close server socket to unblock accept() */
    close(ctx->server_fd);
    
    /* Wait for threads to finish */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(ctx->threads[i], NULL);
    }
    
    free(ctx);
    
    printf("Lua Sandbox Server stopped\n");
}

/*
 * Main Function
 * -----------
 * Entry point for the server application
 */
int main(void) {
    const int port = PORT;
    
    /* Start the server */
    struct server_context *server = start_server(port);
    if (!server) {
        fprintf(stderr, "Failed to start server on port %d\n", port);
        return 1;
    }
    
    printf("Server running. Press Enter to stop.\n");
    
    /* Wait for Enter key */
    getchar();
    
    /* Stop the server */
    stop_server(server);
    
    return 0;
}
