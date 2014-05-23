/*
 * sioux.c - The main() for the sioux webserver; initializes and invokes
 *           the run loop in sioux_run.c.
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <sthread.h>

#include <sioux_run.h>

/* Max length of a hostname */
static const size_t MAX_HOSTNAME = 128;

/* PORT_BASE should be > 1024 (ports below 1024 require root privalege) */
static const int PORT_BASE = 8000;
/* Restrict ports to PORT_BASE -> PORT_BASE+PORT_MAX_INCR */
static const int PORT_MAX_INCR = 8000;

/* The directory to look for files in */
static const char DEFAULT_DOCROOT[] = "./docs";

static int web_getport(void);
static const char *web_gethostname(void);
static const char *web_getdocroot(void);
static void web_printurl(const char *host, int port);
static void usage(void);

int main(int argc, char **argv) {
  if (argc != 2)
    usage();

  int port;
  const char *host;
  const char *docroot;
  int num_threads;

  sthread_init();

  /* Get the configuration information */
  host = web_gethostname();
  port = web_getport();
  docroot = web_getdocroot();
  num_threads = atoi(argv[1]);

  /* Tell the user where to look for this server */
  web_printurl(host, port);

  /* Handle requests forever */
  web_runloop(host, port, docroot, num_threads);
  return 0;
}

/* Try to guess a unique port (if there are lots of students
 * on the same machine, this helps avoid conflicts) */
int web_getport() {
  int port;
  port = PORT_BASE + (geteuid() % PORT_MAX_INCR);
  return port;
}

/* Get the hostname of this machine */
const char *web_gethostname() {
  char *name;
  name = malloc(MAX_HOSTNAME);
  assert(name != NULL);
  if (gethostname(name, MAX_HOSTNAME) == -1) {
    free(name);
    name = NULL;
    perror("sioux: unable to get hostname");
  }

  return name;
}

/* Return the path to the directory containing the files to serve */
const char *web_getdocroot() {
  /* Currently, no way to override default. */
  return DEFAULT_DOCROOT;
}

/* Print a URL the user can use to access this server */
void web_printurl(const char *host, int port) {
  host = (host == NULL) ? "UNKNOWN" : host;
  printf("starting sioux web server (%s threads) on:\n",
         sthread_get_impl() == STHREAD_USER_IMPL ? "user" : "pthread");
  printf("     http://%s:%d\n", host, port);
}

void usage() {
  printf("usage: ./sioux num_threads\n");
  exit(EXIT_FAILURE);
}
