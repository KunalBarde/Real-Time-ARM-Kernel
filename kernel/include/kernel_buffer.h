/**
 * Kernel Buffer Header File
 * Defines API for kernel state manipulation
 */

#include <unistd.h>

/*
Ring buffer defn.
size, head, tail declared volatile due to concurrency of user-space/kernel-space interrupt handler
*/

typedef struct {
    volatile uint32_t size;
    volatile uint32_t n_elems;
    volatile uint32_t head;
    volatile uint32_t tail;
    char payload[0];
}rbuf_t;

/* State manipulation API */
extern void kernel_buffer_init(rbuf_t *buffer, unsigned init_size);
extern int put(rbuf_t *buffer, char c);
extern char poll(rbuf_t *buffer, int *err);
extern void flush(rbuf_t *buffer);

