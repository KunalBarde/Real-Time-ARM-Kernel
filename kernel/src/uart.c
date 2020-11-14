/**

 * @file uart.c
 *
 * @brief UART Interrupt-Based Implementation. Allows kernel user to enable or disable interrupt based UART function. 
 *
 * @date  11/3/2020
 *
 * @author Nick Toldalagi, Kunal Barde
 */

#include <unistd.h>
#include <rcc.h>
#include <gpio.h>
#include <uart.h>
#include <kernel_buffer.h>
#include <nvic.h>
#include <debug.h>

/**
* UART irq number.
*/
#define UART_IRQ 38

/**
* Transmit and receive buffer max sizes. 
*/
#define BUFFER_SIZE 512

/**
* Maximum transmit or receive treshold for guaranteeing worst case interrupt handling time.  
*/
#define THRESHOLD 16


/* Map portion of data section to kernel data structures */
static volatile char recv_buffer[RBUF_SIZE];
static volatile char transmit_buffer[RBUF_SIZE];
static volatile char recv_buffer_payload[BUFFER_SIZE]= {0};
static volatile char transmit_buffer_payload[BUFFER_SIZE] = {0};

/**
* @brief	Initialize interrupt-based UART. 

* @param[in]	baud	The baud at which to initialize UART. 
*/
void uart_init(int baud){
    
    //Init PA_2 UART_2 TX
    gpio_init(GPIO_A, 2, MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    //Init PA_3 UART_2 RX
    gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    //Register in nvic 
    nvic_irq(UART_IRQ, IRQ_ENABLE); 

    /* Initialize kernel buffers */
    kernel_buffer_init((rbuf_t *)recv_buffer, BUFFER_SIZE, recv_buffer_payload);
    kernel_buffer_init((rbuf_t *)transmit_buffer, BUFFER_SIZE, transmit_buffer_payload);
    
    struct uart_reg_map *uart = UART2_BASE;
    struct rcc_reg_map *rcc = RCC_BASE;

    rcc->apb1_enr |= APBCLK_UART_EN;
    uart->CR1 |= UART_TE;
    uart->CR1 |= UART_RE;
    uart->BRR = baud;
    uart->CR1 |= UART_EN;
    uart->CR1 |= UART_RXNE;
    return;
}

/**
* @brief	Put a single byte into the UART transmit buffer. 

* @param	c	The char to be transmitted. 

* @return	0 on success, -1 otherwise. 
*/
int uart_put_byte(char c){
   struct uart_reg_map *uart = UART2_BASE;

   rbuf_t *ring_buffer = (rbuf_t *)transmit_buffer;
   int enq_result = put(ring_buffer, c); 
   uart->CR1 |= UART_TXE;

   return enq_result;
}

/**
* @brief	Attempts to get a single byte from the UART receive buffer. 

* @param[out]	c	The pointer meant for the char returned from a poll of the receive buffer. 

* @return	0 on success or -1 if a poll of the ring buffer failed to retrieve a byte.  
*/
int uart_get_byte(char *c){
   rbuf_t *ring_buffer = (rbuf_t *)recv_buffer;
   int err = 0;
   char polled_byte = poll(ring_buffer, &err);
   if(err) {
      return -1;
   }
   *c = polled_byte;
   return 0;
}

/**
* @brief	Handles uart interrupts triggered both by receive or transmit readiness of the uart. 
*/
void uart_irq_handler(){
   struct uart_reg_map *uart =  UART2_BASE;

   char transmit_byte, recv_byte;
   int err = 0;
   size_t sent_byte_count = 0;
   size_t recv_byte_count = 0;

   rbuf_t *recv_kernel_buffer = (rbuf_t *)recv_buffer;
   rbuf_t *transmit_kernel_buffer = (rbuf_t *)transmit_buffer;

   int read_ready = uart->SR & UART_RXNE;
   int transmit_ready = uart->SR & UART_TXE;
   /* transmit_ready TDR is empty, read_ready RDR is not empty */ 

   /* Transmit DR is empty (Can send) */
   if(transmit_ready) {
      while(sent_byte_count < THRESHOLD) {
         while(!(uart->SR & UART_TXE));
         if(transmit_kernel_buffer -> n_elems == 0) {
           uart->CR1 &= ~UART_TXE;
           break;
         }
         transmit_byte = poll(transmit_kernel_buffer, &err);
         if(err < 0)
            break;
         uart->DR = (unsigned int)transmit_byte;
         sent_byte_count++; 
      }
   } 
   
   /* Recieve if ready */
   if(read_ready) {
      while(recv_byte_count < THRESHOLD) {
         if(!(uart->SR & UART_RXNE)) break;
         recv_byte = (char)uart->DR;
         if(put(recv_kernel_buffer, recv_byte) < 0)
            break;
         recv_byte_count++; 
      } 
   }
   return;
}

/**
* @brief	   Send all bytes remaining in transmit buffer
*/
void uart_flush(){
   /* Send all bytes remaining in transmit buffer */
   char transmit_byte;
   int err;
   
   rbuf_t *transmit_kernel_buffer = (rbuf_t *)transmit_buffer;
   if(transmit_kernel_buffer->n_elems == 0) {
      return;
   }

   struct uart_reg_map *uart = UART2_BASE;
   while(transmit_kernel_buffer->n_elems > 0) {
      while(!(uart->SR & UART_TXE));
      transmit_byte = poll(transmit_kernel_buffer, &err);
      uart->DR = (unsigned int)transmit_byte;
   }
   return;
}
