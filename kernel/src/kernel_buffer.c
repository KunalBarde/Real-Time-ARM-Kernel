/**
 * @brief Implementation of kernel buffer API for use with uart interrupts. 

 * @file	kernel_buffer.c
 * @author	Kunal Barde
 */
#include <kernel_buffer.h>
#include <unistd.h>
#include <errno.h>

/**
 * @brief	Initialize an rbuf_t. 
 
 * @param[in]	buffer	Allocated buffer which should be initialized with the correct values. 
 * @param[in]	init_size	Size of the desired buffer. Should match len(payload)
 * @param[in]	payload	(char*) Allocated memory to be used to story the rbuf_t payload. 
 */
void kernel_buffer_init(rbuf_t *buffer, unsigned init_size, volatile char *payload){
   buffer->head = 0;
   buffer->n_elems = 0;
   buffer->tail = 0;
   buffer->size = init_size;
   buffer->payload = payload;
}

/**
 * @brief	Put a char in to a buffer. 
 
 * @param[in]	buffer	The buffer which the char should be added to. 
 * @param[in]	c	Char to add. 

 * @return	0 on success. -1 otherwise. 
 */
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

/** 
 * @brief	Poll a buffer for a single char. 

 * @param[in]	buffer	The buffer to be polled.
 * @param[out]	err	(int *) 0 if a char was successfully retrieved, 0 otherwise. 

 * @return	retrieved cahr. 
 */
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

