/**
 * @file 
 *
 * @brief      
 *
 * @date       
 *
 * @author     
 */

#ifndef _UART_H_
#define _UART_H_

struct uart_reg_map {
    volatile uint32_t SR;   /**< Status Register */
    volatile uint32_t DR;   /**<  Data Register */
    volatile uint32_t BRR;  /**<  Baud Rate Register */
    volatile uint32_t CR1;  /**<  Control Register 1 */
    volatile uint32_t CR2;  /**<  Control Register 2 */
    volatile uint32_t CR3;  /**<  Control Register 3 */
    volatile uint32_t GTPR; /**<  Guard Time and Prescaler Register */
};

typedef struct {
    volatile uint32_t size;
    volatile uint32_t head;
    volatile uint32_t tail;
    char *payload;
}r_buf_t;


/** @brief Base address for UART2 */
#define UART2_BASE  (struct uart_reg_map *) 0x40004400
#define UART_EN (1 << 13)
#define UART_RE (1 << 2)
#define UART_TE (1 << 3)
#define UART_TXE (1 << 7)
#define UART_RXNE (1 << 5)
#define USART_DIV 0x008B 
#define APBCLK_UART_EN (1 << 17)


void uart_init(int baud);

int uart_put_byte(char c);

int uart_get_byte(char *c);

void uart_flush();

#endif /* _UART_H_ */
