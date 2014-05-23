#include <config.h>

#include <string.h>

#include <sthread.h>

sthread_impl_t sthread_get_impl(void) {
#ifdef USE_PTHREADS
  return STHREAD_PTHREAD_IMPL;
#else
  return STHREAD_USER_IMPL;
#endif
}
