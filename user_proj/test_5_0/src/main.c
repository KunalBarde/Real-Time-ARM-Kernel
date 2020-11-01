/**
 * @file   main.c
 *
 * @brief  Test erroneous read/write detection.
 */

#include <349_lib.h>
#include <349_threads.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/** @brief thread user space stack size - 1KB */
#define USR_STACK_WORDS 256
#define NUM_THREADS 1
#define NUM_MUTEXES 0
#define CLOCK_FREQUENCY 1000

#define UNUSED __attribute__((unused))

// Attempt a buffer overflow attack
void thread_function( UNUSED void *vargp ) {
  int buffer[ USR_STACK_WORDS / 2 ];
  int i = 0;

  while( 1 ) {
    printf( "i=%d\tAddr=%p\tVal=%d\n", i, &buffer[i], buffer[i] );
    i++;
  }
}

int main() {

  ABORT_ON_ERROR( thread_init( NUM_THREADS, USR_STACK_WORDS, NULL, PER_THREAD, NUM_MUTEXES ) );

  ABORT_ON_ERROR( thread_create( &thread_function, 0, 400, 500, NULL ) );

  printf( "Starting scheduler...\n" );

  ABORT_ON_ERROR( scheduler_start( CLOCK_FREQUENCY ) );
  
  printf( "There should be a data access violation\n\tclose to i=132; if so, test passed.\n" );

  return 0;

}
