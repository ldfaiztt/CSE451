/*
 * sioux_run.c - Implements the main loop for the webserver, processing
 *               requests and sending replies.
 *
 */

#include <config.h>

#include <assert.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sthread.h>

#include <sioux_run.h>
#include <thread_pool.h>

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* Every http response includes a numeric status code indicating,
 * to the browser, what the result was. These are the codes we are
 * interested in.
 */
typedef enum _status {
  STATUS_200_OK = 200,
  STATUS_400_BAD_REQUEST = 400,
  STATUS_404_NOT_FOUND = 404,
  STATUS_405_METHOD_NOT_ALLOWED = 405
} status_t;

/* How many connections can be waiting, but not accepted,
 * before the kernel starts refusing new connections.
 */
static const int BACKLOG = 5;

/* Requests really do get this big: */
static const int REQUEST_MAX_SIZE = 4096;

/* How much of the file to read at a time. */
static const int BUFFER_SIZE = 4096;

static const char CRLF[] = "\r\n";
static const char REQUEST_TERMINATOR[] = "\r\n\r\n";
static const char REQUEST_TERMINATOR_LOOSE[] = "\n\n";
static const char SERVER[] = "Sioux/1.0 (Unix)";
static const char HTTP_VERSION[] = "HTTP/1.1";
static const char INDEX_FILE[] = "index.html";

static int web_setup_socket(int port);
static int web_next_connection(int listen_socket);
void web_handle_connection(int conn, const char *docroot);
static int web_read_request(int conn, char *request_buf, size_t size);
static status_t web_parse_request(char *request_buf, char *filename,
                                  size_t filename_len, const char *docroot);
static void web_send_headers(FILE* stream, status_t status);
static const char *web_get_status_string(status_t status);
static status_t web_open_file(const char *filename, FILE **file);
static void web_send_file(FILE *stream, FILE *file);
static void web_send_error_doc(FILE *stream, status_t status);


/* Run the webserver. Our host is given, as well as the port to listen
 * on, and the directory that the documents can be found in.
 * Runs forever.
 */
void web_runloop(const char *host, int port, const char *docroot, int num_threads) {
  int listen_socket, next_conn;

  listen_socket = web_setup_socket(port);

  // initialize a thread pool to handle connections
  thread_pool* tp = thread_pool_init(num_threads, docroot);
  assert(tp != NULL);

  while ((next_conn = web_next_connection(listen_socket)) >= 0) {
    int *conn_ptr = (int *) malloc(next_conn);
    assert(conn_ptr != NULL);
    *conn_ptr = next_conn;

    // add a connection to request queue and wake up
    // a thread to handle the connection
    thread_pool_dispatch(tp, conn_ptr);
  }

  close(listen_socket);
}


/* Create a new socket that is bound to the given port, ready
 * to accpet connections. Aborts on failure. */
int web_setup_socket(int port) {
  int listen_socket;
  struct sockaddr_in listen_addr;

  listen_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (listen_socket == -1) {
    perror("sioux: failed to create socket");
    abort();
  }

  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons((uint16_t) port);
  listen_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(listen_socket, (struct sockaddr*)&listen_addr,
           sizeof(struct sockaddr_in)) == -1) {
    perror("sioux: failed to bind socket");
    close(listen_socket);
    abort();
  }

  if (listen(listen_socket, BACKLOG) == -1) {
    perror("sioux: failed to listen");
    close(listen_socket);
    abort();
  }

  return listen_socket;
}

/* Get the next incoming connection from the given socket,
 * which should be bound and listening for connections.
 * Will block until a connection is available. Return
 * -1 on error, 0 or greater on success.
 * This function is not thread safe - multiple threads should
 * not invoke it simultaneously. */
int web_next_connection(int listen_socket) {
  int next_conn;
  struct sockaddr_in addr;
  socklen_t len = sizeof(struct sockaddr_in);

  next_conn = accept(listen_socket, (struct sockaddr*)&addr, &len);
  if (next_conn == -1)
    perror("sioux: error accepting connections");

  return next_conn;
}

/* Do all the actual request handling.
 * Read in the request, parse it, and send the requested file
 * back (or send an error back) */
void web_handle_connection(int conn, const char *docroot) {
  FILE *stream = NULL, *file = NULL;
  char *request_buf, *filename;
  status_t status;
  request_buf = malloc(REQUEST_MAX_SIZE);
  assert(request_buf != NULL);
  filename = malloc(REQUEST_MAX_SIZE);
  assert(filename != NULL);

  if (web_read_request(conn, request_buf, REQUEST_MAX_SIZE) == -1) {
    fprintf(stderr, "error reading request");
    goto done;
  }

  stream = fdopen(conn, "w");
  if (stream == NULL) {
    /* Couldn't open a stream; no way to send a response to the client. */
    perror("sioux: stream fdopen error");
    goto done;
  }

  /* Get the filename out of the request. */
  status = web_parse_request(request_buf, filename, REQUEST_MAX_SIZE, docroot);

  if (status != STATUS_200_OK) {
    fprintf(stderr, "request error %d\n", status);
    web_send_headers(stream, status);
    web_send_error_doc(stream, status);
    goto done;
  }

  /* See if we can find this file */
  status = web_open_file(filename, &file);

  if (status != STATUS_200_OK) {
    fprintf(stderr, "request error %d\n", status);
    web_send_headers(stream, status);
    web_send_error_doc(stream, status);
    goto done;
  }

  /* Finally - send the file */
  web_send_headers(stream, status);
  web_send_file(stream, file);
  fflush(stream);
  fclose(file);

 done:
  if (stream != NULL)
    fclose(stream);
  free(request_buf);
  free(filename);
}

/* Read a request from the conn into request_buf, not more than
 * size bytes long. Return 0 on success, -1 on error.
 */
int web_read_request(int conn, char *request_buf, size_t size) {
  ssize_t count = 0, rd;
  /* save 1 char for the '\0' terminator */
  while ((rd = read(conn, request_buf + count, size-1 - count))) {
    if (rd == -1) {
      perror("sioux: read error");
      return -1;
    }
    count += rd;
    if (count >= (size-1)) {
      request_buf[count] = '\0';
      fprintf(stderr, "request: %s\n", request_buf);
      fprintf(stderr, "sioux: request too large\n");
      return -1;
    }

    request_buf[count] = '\0';
    if (strstr(request_buf, REQUEST_TERMINATOR) != 0 ||
        strstr(request_buf, REQUEST_TERMINATOR_LOOSE) != 0) {
      return 0;
    }
  }
  /* End-of-file without a blank line; bad request */
  return -1;
}

/* Parse the given request, possibly modifying it.
 * Return a the actual filename to be fetched from disk */
status_t web_parse_request(char *request, char *filename,
                           size_t filename_len,
                           const char *docroot) {
  char *end_of_name;
  char *tmp;

  /* look for "GET" at the very beginning of the request */
  if (strstr(request, "GET ") != request)
    /* We only support GET requests
     * (not POST, nor any of the stranger types) */
    return STATUS_405_METHOD_NOT_ALLOWED;
  request += strlen("GET ");

  /* Requests may or may not include "http://servername:port/";
   * if they do, take it off here. */
  if ((tmp = strstr(request, "http://")) != NULL) {
    request = tmp;
    request += strlen("http://");
    request = strchr(request, '/');
    if (request == NULL)
      return STATUS_400_BAD_REQUEST;
  }

  /* request is now advanced to what should be the filename itself,
   * search for the end of it */
  end_of_name = strchr(request, ' ');
  if (end_of_name == NULL)
    return STATUS_400_BAD_REQUEST;
  *end_of_name = '\0';

  /* If filename ends in '/', tack on the "index.html"
   * This is a poor heuristic - should really check if the filename
   * is a directory. */
  snprintf(filename, filename_len, "%s%s%s", docroot, request,
           request[strlen(request) - 1] == '/' ? INDEX_FILE : "");

  return STATUS_200_OK;
}

/* Every http response must begin with a set of headers, indicating
 * at least the version of the protocol and code for what happened
 */
void web_send_headers(FILE *stream, status_t status) {
  fprintf(stream, "%s %d %s\r\n", HTTP_VERSION, status,
          web_get_status_string(status));
  fprintf(stream, "Server: %s\r\n", SERVER);
  fprintf(stream, "Content-Type: text/html\r\n");
  fprintf(stream, "Connection: close\r\n");
  fprintf(stream, CRLF);
}

/* Open a file. Return a status code indicating success (200) or failure
 * (anything else) */
status_t web_open_file(const char *filename, FILE **file) {
  *file = fopen(filename, "r");
  if (*file == NULL)
    return STATUS_404_NOT_FOUND;
  printf("sending file: %s\n", filename);
  return STATUS_200_OK;
}

/* Given an open stream to send to, and an open file to read from,
 * transfer the file. */
void web_send_file(FILE *stream, FILE *file) {
  size_t count;
  char *buf;
  buf = (char*)malloc(BUFFER_SIZE);
  assert(buf != NULL);

  while ((count = fread(buf, 1, BUFFER_SIZE, file)) != 0) {
    //    fprintf(stderr, "sending file: %d\n", (int)count);
    count = fwrite(buf, 1, count, stream);
    if (count == 0) {
      fprintf(stderr, "error sending file\n");
      break;
    }
  }

  free(buf);
}

/* Send an html document describing the error that occurred. */
void web_send_error_doc(FILE *stream, status_t status) {
  fprintf(stream, "<html><head><title>Error %d</title></head>\n", status);
  fprintf(stream, "<body><h1>Error %d: %s</h1></body></html>\n", status,
          web_get_status_string(status));
}

/* Each status number has an associated string. Return it. */
const char *web_get_status_string(status_t status) {
  switch (status) {
  case STATUS_200_OK:
    return "OK";
  case STATUS_400_BAD_REQUEST:
    return "Bad Request";
  case STATUS_404_NOT_FOUND:
    return "Not Found";
  case STATUS_405_METHOD_NOT_ALLOWED:
    return "Method Not Allowed";
  }
  abort();
  return NULL;
}


