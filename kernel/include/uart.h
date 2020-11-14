/**
 * @file uart.h
 *
 * @brief      Definitions for uart interrupt-based device driver.
 *
 * @date       
 *
 * @author     
 */

#ifndef _UART_H_
#define _UART_H_

/**
 * @struct	Uart functionality register map 
 */
struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**<  Data Register */
    volatile uint32_t BRR;  /**<  Baud Rate Register */
    volatile uint32_t CR1;  /**<  Control Register 1 */
    volatile uint32_t CR2;  /**<  Control Register 2 */
    volatile uint32_t CR3;  /**<  Control Register 3 */
    volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
};

/** 
 * @struct	Struct definition for a ring buffer used used for the uart interrupt implementation. 
 */
typedef struct {
    volatile uint32_t size; /**< Size of buffer in bytes*/
    volatile uint32_t head; /**< Current head index of buffer */
    volatile uint32_t tail; /**< Current tail index of buffer */
    char *payload; /**< Pointer to payload (data) of the buffer */
}r_buf_t;


#define UART2_BASE  (struct uart_reg_map *) 0x40004400 /**< @brief Base address for UART2 */
#define UART_EN (1 << 13) /**< Enable uart bit */
#define UART_RE (1 << 2) /**< Receive enable bit*/
#define UART_TE (1 << 3) /**< Transmit enable bit*/
#define UART_TXE (1 << 7) /**< Receive ready bit*/
#define UART_RXNE (1 << 5) /**< Transmit ready bit*/
#define APBCLK_UART_EN (1 << 17) /**< Utilize APB clk for uart bit */

#define USART_DIV 0x008B /**< Desired uart baud rate. */


/** @brief	Initialize uart device driver */
void uart_init(int baud);

/** @brief	Put a single byte into the uart */
int uart_put_byte(char c);

/** @brief	Recieve a single byte from the uart */
int uart_get_byte(char *c);

/** @brief	Flush the uart buffers */
void uart_flush();

#endif /* _UART_H_ */
