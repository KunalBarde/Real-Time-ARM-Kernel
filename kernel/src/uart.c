/**

 * @file UART Interrupt-Based Implementation 
 *
 * @brief
 *
 * @date  
 *
 * @author     
 */

#include <unistd.h>
#include <rcc.h>
#include <gpio.h>
#include <uart.h>
#include <kernel_buffer.h>
#include <nvic.h>
#include <debug.h>

#define UNUSED __attribute__((unused))
#define UART_IRQ 38
#define BUFFER_SIZE 512
#define THRESHOLD 16

/* Map portion of data section to kernel data structures */
static volatile char recv_buffer[BUFFER_SIZE];
static volatile char transmit_buffer[BUFFER_SIZE];

void uart_init(UNUSED int baud){
    //Init PA_2 UART_2 TX
    gpio_init(GPIO_A, 2, MODE_ALT, OUTPUT_PUSH_PULL, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    //Init PA_3 UART_2 RX
    gpio_init(GPIO_A, 3, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT7);

    //Register in nvic 
    nvic_irq(UART_IRQ, IRQ_ENABLE); 

    /* Initialize kernel buffers */
    kernel_buffer_init((rbuf_t *)recv_buffer, BUFFER_SIZE);
    kernel_buffer_init((rbuf_t *)transmit_buffer, BUFFER_SIZE);
    
    struct uart_reg_map *uart = UART2_BASE;
    struct rcc_reg_map *rcc = RCC_BASE;

    rcc->apb1_enr |= APBCLK_UART_EN;
    uart->CR1 |= UART_TE;
    uart->CR1 |= UART_RE;
    uart->BRR = USART_DIV;
    uart->CR1 |= UART_EN;
    uart->CR1 |= UART_RXNE;
    return;
}

int uart_put_byte(UNUSED char c){
   struct uart_reg_map *uart = UART2_BASE;
   rbuf_t *ring_buffer = (rbuf_t *)transmit_buffer;
   int enq_result = put(ring_buffer, c); 
   uart->CR1 |= UART_TXE;
   return enq_result;
}

int uart_get_byte(UNUSED char *c){
   UNUSED struct uart_reg_map *uart = UART2_BASE;
   rbuf_t *ring_buffer = (rbuf_t *)recv_buffer;
   int err = 0;
   char polled_byte = poll(ring_buffer, &err);
   if(err) {
      return -1;
   }
   *c = polled_byte;
   return 0;
}

void uart_irq_handler(){
   struct uart_reg_map *uart =  UART2_BASE;
   /* Disable Interrupts (Entering Critical Section) */
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
