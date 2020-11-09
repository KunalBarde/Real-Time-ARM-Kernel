#include <gpio.h>
#include <i2c.h>
#include <unistd.h>
#include <rcc.h>

/** @brief The I2C register map. */
struct i2c_reg_map {
    volatile uint32_t CR1;   /**<  Control Register 1 */
    volatile uint32_t CR2;   /**<  Control Register 2 */
    volatile uint32_t OAR1;  /**<  Own Address register 1*/
    volatile uint32_t OAR2;  /** Own address register 2 */
    volatile uint32_t DR;  /**<  Data Register */
    volatile uint32_t SR1;  /**< Status Register 1 */ 
    volatile uint32_t SR2; /**< Status Register 2 */
    volatile uint32_t CCR; /** CLock control register */
    volatile uint32_t TRISE; /** Rise time regsiter */
    volatile uint32_t FLTR;  /** Filter Register */
};

/** @brief Initializes I2C1. 
    @param clk Precomputed CCR constant for configuring the i2c clock to desired rate.*/
void i2c_master_init(uint16_t clk){
    //(void) clk; /* This line is simply here to suppress the Unused Variable Error. */
                /* You should remove this line in your final implementation */
    
    //Init PB_8 as I2C SCL
    gpio_init(GPIO_B, 8, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);

    // Init PB_9 as I2C SDA 
    gpio_init(GPIO_B, 9, MODE_ALT, OUTPUT_OPEN_DRAIN, OUTPUT_SPEED_LOW, PUPD_NONE, ALT4);
    
    struct i2c_reg_map *i2c = I2C1_BASE;
    struct rcc_reg_map *rcc = RCC_BASE;
    
    rcc -> apb1_enr |= APBCLK_I2C_EN;

    i2c -> CR2 |= APBCLK_FREQ; 
    i2c -> CR1 &= (!I2C_EN); //Make sure disabled when setting CCR

    i2c -> CCR &= !(LWR_12BITS); //Clear 12 lower bits 
    i2c -> CCR |= clk; 

    i2c -> TRISE &= !(LWR_5BITS); //Clear 5 lower bits
    i2c -> TRISE |= I2C_TRISE;

    i2c -> CR1 |= I2C_EN;

    return;
}

/** @brief Sends start condition over I2C and waits for confirmation of successful sending */
void i2c_master_start(){
    struct i2c_reg_map *i2c = I2C1_BASE;

    i2c -> CR1 = i2c -> CR1 | I2C_START;
    
    //Wait for EV5
    while (!(i2c -> SR1 & SR_SB)); //Wait for SR to indicate start sent 

    return;
}

/** @brief Sends stop condition over I2C and waits for confirmation of master returning to slave mode. */
void i2c_master_stop(){
    struct i2c_reg_map *i2c = I2C1_BASE;
    i2c -> CR1 |= I2C_STOP;
    
    while (i2c -> SR2 & SR_MSL);
    return;
}

/** @brief Writes bytes to I2C one bus along with slave address 
    @param buf Buffer of bytes to write.
    @param len Number of bytes from buf to write. 
    @param slave_addr 7-bit address of slave to write to. The addr should be in the top 7 bits with the lsb left open for indicating a read or write. */
int i2c_master_write(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    struct i2c_reg_map *i2c = I2C1_BASE;

    //Write addr
    i2c -> DR = (uint32_t) slave_addr;

    //Wait for EV6
    while (!(i2c -> SR1 & SR_ADDR));
    i2c -> SR2; // Read SR2 to clear ADDR

    //Wait for EV8_1
    while (!(i2c -> SR1 & SR_TxE));

    for (int i = 0; i < len; i++) {
       i2c -> DR = buf[i];
       //Wait for EV8
       while (!(i2c -> SR1 & SR_TxE));
    }

    uint32_t sr = i2c -> SR1;

    //Wait for EV_2
    while (!((sr & SR_TxE) && (sr & SR_BTF))) {
    	sr = i2c -> SR1;	
    }

    return 0;
}

/** @brief Reads buffer of bytes from i2c slave. 
    @param buf Buffer in which the bytes will be read into.
    @param len Number of bytes to be read into buf. 
    @slave_addr Address of slave from which to read. */
int i2c_master_read(uint8_t *buf, uint16_t len, uint8_t slave_addr){
    struct i2c_reg_map *i2c = I2C1_BASE;

    //Pad for read
    slave_addr = slave_addr | 1;

    i2c_master_start();

    //Write addr
    i2c -> DR = (uint32_t) slave_addr;
 
    //Wait for EV6
    while (!(i2c -> SR1 & SR_ADDR));
    i2c -> SR2; // Read SR2 to clear ADDR

    for (int i = 0; i < len-2; i++) {
      
      buf[i] = i2c -> DR & LWR_8BITS;
      i2c -> CR1 |= I2C_ACK; 
      
      //Waiting for EV7
      while (!(i2c -> SR1 & SR_RxNE));
    }

    buf[len-2] = i2c -> DR & LWR_8BITS;
    while (!(i2c -> SR1 & SR_RxNE));
    i2c -> CR1 &= !(I2C_ACK);  

    while (!(i2c -> SR1 & SR_RxNE));
    i2c_master_stop();
    buf[len-1] = i2c -> DR & LWR_8BITS;
    
    return 0;
}
