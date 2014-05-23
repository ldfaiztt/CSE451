#ifndef SIOUX_RUN_H
#define SIOUX_RUN_H 1

void web_runloop(const char *host, int port, const char *docroot, int num_threads);

void web_handle_connection(int conn, const char *docroot);

#endif /* SIOUX_RUN_H */
