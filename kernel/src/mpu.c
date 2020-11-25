/** @file    mpu.c
 *
 *  @brief   MPU interface implementation for thread-wise
 *           memory protection.
 *
 *  @date    30 Mar 2020
 *
 *  @author  Benjamin Huang <zemingbh@andrew.cmu.edu>
 */

#include "arm.h"
#include "debug.h"
#include "printk.h"
#include "syscall.h"
#include "mpu.h"
#include "syscall_thread.h"

/**Compiler macro used to indicate arguments which should be temporarily ignored as they are unused*/
#define UNUSED __attribute__((unused))

/**
 * @struct mpu_t
 * @brief  MPU MMIO register layout.
 */
typedef struct {
  /**@brief provides information about the MPU */
  volatile uint32_t TYPER;
  /**@brief MPU enable/disable and background region control. */
  volatile uint32_t CTRL;
  /** @brief Select which MPU region to be configured*/
  volatile uint32_t RNR;
  /**@brief Defines base address of a MPU region */
  volatile uint32_t RBAR;
  /**@brief Defines size and attribues of a MPU region*/
  volatile uint32_t RASR;

  /**@brief Field aliases. */
  //@{
  volatile uint32_t RBAR_A1;
  volatile uint32_t RASR_A1;
  volatile uint32_t RBAR_A2;
  volatile uint32_t RASR_A2;
  volatile uint32_t RBAR_A3;
  volatile uint32_t RASR_A3;
  //@}
} mpu_t;

/**
 * @struct system_control_block_t
 * @brief  System control block register layout.
 */
typedef struct {
  /**@brief System handler control and state register.*/
  volatile uint32_t SHCRS;
  /**@brief Configurable fault status register.*/
  volatile uint32_t CFSR;
  /**@brief HardFault Status Register */
  volatile uint32_t HFSR;
  /**@brief Hint information of debug events.*/
  volatile uint32_t DFSR;
  /**@brief Addr value of memfault*/
  volatile uint32_t MMFAR;
  /**@brief Addr of bus fault */
  volatile uint32_t BFAR;
  /**@brief Auxilliary fault status register.*/
  volatile uint32_t AFSR;
} system_control_block_t;

/**@brief MPU base address.*/
#define MPU_BASE ( ( mpu_t * )0xE000ED90 );

/** @brief MPU CTRL register flags. */
//@{
#define CTRL_ENABLE_BG_REGION ( 1<<2 )
#define CTRL_ENABLE_PROTECTION ( 1<<0 )
//@}

/** @brief MPU RNR register flags. */
#define RNR_REGION ( 0xFF )
/** @brief Maximum region number. */
#define REGION_NUMBER_MAX 7

/** @brief MPU RBAR register flags. */
//@{
#define RBAR_VALID ( 1<<4 )
#define RBAR_REGION ( 0xF )
//@}

/** @brief MPU RASR register masks. */
//@{
#define RASR_XN ( 1<<28 )
#define RASR_AP_KERN ( 1<<26 )
#define RASR_AP_USER ( 1<<25 | 1<<24 )
#define RASR_SIZE ( 0b111110 )
#define RASR_ENABLE ( 1<<0 )
//@}

/** @brief MPU RASR AP user mode encoding. */
//@{
#define RASR_AP_USER_READ_ONLY ( 0b10<<24 )
#define RASR_AP_USER_READ_WRITE ( 0b11<<24 )
//@}

/**@brief Systen control block MMIO location.*/
#define SCB_BASE ( ( volatile system_control_block_t * )0xE000ED24 )

/**@brief Enable memory faults */
#define MEMFAULT_EN 0x1 << 16

/**@brief Memory region attribute read only. */
#define READ_ONLY 0
/**@brief Memory region attribute is executable by user */
#define EXECUTABLE 1

/**@brief Stacking error.*/
#define MSTKERR 0x1 << 4
/**@brief Unstacking error.*/
#define MUNSTKERR 0x1 << 3
/**@brief Data access error.*/
#define DACCVIOL 0x1 << 1
/**@brief Instruction access error.*/
#define IACCVIOL 0x1 << 0
/**@brief Indicates the MMFAR is valid.*/
#define MMARVALID 0x1 << 7
/**@brief System word size*/
#define WORD_SIZE 4

/**
 * @brief	User memory region addresses
 */
//@{
extern char
  _swi_stub_start,
  _u_rodata,
  _u_data,
  _u_bss,
  __heap_low,
  __psp_stack_bottom;
//@}

/**
 * @brief	Thread stack memory region addresses
 */
//@{
extern char
  __thread_u_stacks_top, 
  __thread_k_stacks_top,
  __thread_u_stacks_low,
  __thread_k_stacks_low;
//@}

/**
 * @brief	Memory managment interrupt c handler
 */ 
void mm_c_handler( void *psp ) {

  system_control_block_t *scb = ( system_control_block_t * )SCB_BASE;
  int status = scb->CFSR & 0xFF;

  // Attempt to print cause of fault
  DEBUG_PRINT( "Memory Protection Fault\n" );
  WARN( !( status & MSTKERR ), "Stacking Error\n" );
  WARN( !( status & MUNSTKERR ), "Unstacking Error\n" );
  WARN( !( status & DACCVIOL ), "Data access violation\n" );
  WARN( !( status & IACCVIOL ), "Instruction access violation\n" );
  WARN( !( status & MMARVALID ), "Faulting Address = %x\n", scb->MMFAR );

  // You cannot recover from stack overflow because the processor has
  // already pushed the exception context onto the stack, potentially
  // clobbering whatever is in the adjacent stack.
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  uint32_t stack_size = ksb->stack_size;
  uint32_t stack_size_bytes = (1<<(mm_log2ceil_size(stack_size*WORD_SIZE)));
  
  uint32_t process_bottom = (ksb->running_thread + 1)*stack_size_bytes;
  if ((uint32_t)psp < process_bottom) {
    DEBUG_PRINT( "Stack Overflow, aborting\n" );
    sys_exit( -1 );
  }

  if (ksb->running_thread >= ksb->max_threads) { //Idle or main
    sys_exit(-1);
  }

  sys_thread_kill();
}

/**
 * @brief	Enables mpu function and background region protection. 
 
 * @param[in]	enable	if 1 enables mpu. If 0, disables.
 */
void mm_enable_mpu(int enable) {

  //Enable mem fault
  volatile system_control_block_t *scb = SCB_BASE;
  scb -> SHCRS |= MEMFAULT_EN; 

  volatile mpu_t *mpu = MPU_BASE;
  if(enable) 
    mpu -> CTRL |= (CTRL_ENABLE_PROTECTION | CTRL_ENABLE_BG_REGION);
  else 
    mpu -> CTRL &= ~CTRL_ENABLE_PROTECTION;
}

/**
 * @brief	Enable user space mpu protections against access kernel regions. Does not provide mpu protection of kernel stacks. 

 * @return	0 if success. Anything else indicates an error. 
 */
int mm_enable_user_access() {
  
  void *user_text_start = (void *)&_swi_stub_start;
  void *user_rodata = (void *)&_u_rodata;
  void *user_data = (void *)&_u_data;
  void *user_bss = (void *)&_u_bss;
  void *heap_low = (void *)&__heap_low;
  void *user_stack = (void *)&__psp_stack_bottom;
  int err = 0;

  //User code
  err |= mm_region_enable(0, user_text_start, mm_log2ceil_size(16000), EXECUTABLE, READ_ONLY);

  //User rodata
  err |= mm_region_enable(1, user_rodata, mm_log2ceil_size(2000), !EXECUTABLE, READ_ONLY);

  //User data
  err |= mm_region_enable(2, user_data, mm_log2ceil_size(1000), !EXECUTABLE, !READ_ONLY);

  //User bss
  err |= mm_region_enable(3, user_bss, mm_log2ceil_size(1000), !EXECUTABLE, !READ_ONLY);

  //User heap
  err |= mm_region_enable(4, heap_low, mm_log2ceil_size(4000), !EXECUTABLE, !READ_ONLY);

  //Default user stack
  err |= mm_region_enable(5, user_stack, mm_log2ceil_size(2000), !EXECUTABLE, !READ_ONLY);

  return err; 
}

/** 
 * @brief	Enable user thread stack access based on memory access mode. 
 */
int mm_enable_user_stacks(void *process_stack, void *kernel_stack, int thread_num) {
  k_threading_state_t *ksb = (k_threading_state_t *)kernel_threading_state;

  uint32_t stack_size = ksb->stack_size;
  uint32_t stack_size_bytes = (1<<(mm_log2ceil_size(stack_size*WORD_SIZE)));

  uint32_t log2_stack_size = mm_log2ceil_size(stack_size_bytes);
  UNUSED uint32_t log2_all_stacks_size = mm_log2ceil_size(stack_size_bytes*(ksb->u_thread_ct+2));

  uint32_t user_stack_top = (uint32_t)&__thread_u_stacks_top;
  uint32_t kernel_stack_top = (uint32_t)&__thread_k_stacks_top;

  if(thread_num < 0) { //Kernel only

    if(mm_region_enable(6, (void *)&__thread_u_stacks_low, 15, !EXECUTABLE, !READ_ONLY) < 0) return -1;

    if(mm_region_enable(7, (void *)&__thread_k_stacks_low, 
15, !EXECUTABLE, !READ_ONLY) < 0) return -1;

  } else { //Per thread
   
    uint32_t process_bottom = user_stack_top - ((user_stack_top - (uint32_t)process_stack)/stack_size_bytes + 1)*stack_size_bytes;
    uint32_t kernel_bottom = kernel_stack_top - ((kernel_stack_top - (uint32_t)kernel_stack)/stack_size_bytes + 1)*stack_size_bytes;

    if(mm_region_enable(6, (void *)process_bottom, log2_stack_size, !EXECUTABLE, !READ_ONLY) < 0) return -1;

    if(mm_region_enable(7, (void *)kernel_bottom, log2_stack_size, !EXECUTABLE, !READ_ONLY) < 0) return -1;
  }
  return 0;
}

/**
 * @brief	Disable current user thread stack regions. Always 6 and 7. 
 */
void mm_disable_user_stacks() {
  mm_region_disable(6);
  mm_region_disable(7);
}

/**
 * @brief	Disable user accessible memory regions. All memory under background protection now. 
 */ 
void mm_disable_user_access() {
  for(int i = 0; i < 8; i++) {
    mm_region_disable(i);
  }
}

/**
 * @brief  Enables a memory protection region. Regions must be aligned!
 *
 * @param  region_number      The region number to enable.
 * @param  base_address       The region's base (starting) address.
 * @param  size_log2          log[2] of the region size.
 * @param  execute            1 if the region should be executable by the user.
 *                            0 otherwise.
 * @param  user_write_access  1 if the user should have write access, 0 if
 *                            read-only
 *
 * @return 0 on success, -1 on failure
 */
int mm_region_enable(
  uint32_t region_number,
  void *base_address,
  uint8_t size_log2,
  int execute,
  int user_write_access
){
  if (region_number > REGION_NUMBER_MAX) {
    printk("Invalid region number\n");
    return -1;
  }

  if ((uint32_t)base_address & ((1 << size_log2) - 1)) {
    printk("Misaligned region\n");
    return -1;
  }

  if (size_log2 < 5) {
    printk("Region too small\n");
    return -1;
  }

  mpu_t *mpu = MPU_BASE;

  mpu->RNR = region_number & RNR_REGION;
  mpu->RBAR = (uint32_t)base_address;

  uint32_t size = ((size_log2 - 1) << 1) & RASR_SIZE;
  uint32_t ap = user_write_access ? RASR_AP_USER_READ_WRITE : RASR_AP_USER_READ_ONLY;
  uint32_t xn = execute ? 0 : RASR_XN;

  mpu->RASR = size | ap | xn | RASR_ENABLE;

  return 0;
}

/**
 * @brief  Disables a memory protection region.
 *
 * @param  region_number      The region number to disable.
 */
void mm_region_disable( uint32_t region_number ){
  mpu_t *mpu = MPU_BASE;
  mpu->RNR = region_number & RNR_REGION;
  mpu->RASR &= ~RASR_ENABLE;
}

/**
 * @brief  Returns ceiling (log_2 n).
 */
uint32_t mm_log2ceil_size (uint32_t n) {
  uint32_t ret = 0;
  while (n > (1U << ret)) {
    ret++;
  }
  return ret;
}
