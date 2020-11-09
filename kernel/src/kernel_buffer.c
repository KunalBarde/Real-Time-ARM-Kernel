/* Implementation of kernel buffer API */
#include <kernel_buffer.h>
#include <unistd.h>
#include <errno.h>

void kernel_buffer_init(rbuf_t *buffer, unsigned init_size) {
   buffer->head = 0;
   buffer->n_elems = 0;
   buffer->tail = 0;
   buffer->size = init_size;
}

int put(rbuf_t *buffer, char c) {
   
   /* Check if buffer is full */
   if(buffer->n_elems >= buffer->size) {
      return -1;
   }

   buffer->payload[buffer->head++] = c;
   buffer->n_elems++;  

   /* End of buffer */
   if(buffer->head >= buffer->size) {
      buffer->head = 0;
   }
   return 0;
}

char poll(rbuf_t *buffer, int *err) {

   /* Check if buffer is empty */
   if(!(buffer->n_elems)) {
      *err = 1;
      return (char)-1;
   }
   
   char byte = buffer->payload[buffer->tail];
   buffer->payload[buffer->tail] = 0;
   buffer->tail++;
   buffer->n_elems--;

   /* If tail is over the bounds wrap back */
   if(buffer->tail >= buffer->size) {
      buffer->tail = 0;
   }

   *err = 0;
   return byte;
}

