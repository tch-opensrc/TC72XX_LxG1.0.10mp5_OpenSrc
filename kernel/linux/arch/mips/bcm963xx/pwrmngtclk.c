/******************************************************************************
 * Copyright 2009-2012 Broadcom Corporation
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php,
 * or by writing to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
 *
 *****************************************************************************/
#include <linux/module.h>
#include <asm/time.h>
#include <bcm_map_part.h>


#define CLK_ALIGNMENT_REG   0xff410040
#define KEEPME_MASK         0x00007F00 // bit[14:8]

#define RATIO_ONE_SYNC      0x0 /* 0b000 */
#define RATIO_ONE_ASYNC     0x1 /* 0b001 */
#define RATIO_ONE_HALF      0x3 /* 0b011 */
#define RATIO_ONE_QUARTER   0x5 /* 0b101 */
#define RATIO_ONE_EIGHTH    0x7 /* 0b111 */

#define MASK_ASCR_BITS 0x7
#define MASK_ASCR_SHFT 28
#define MASK_ASCR (MASK_ASCR_BITS << MASK_ASCR_SHFT)

unsigned int keepme;
unsigned int cpu_has_low_activity = 0;
unsigned int clock_divide_enabled = 0;
unsigned int clock_divide_low_power = 0;
unsigned int clock_divide_active = 0;
unsigned int self_refresh_enabled = 0;
unsigned int wait_count = 0;
unsigned int prevTimerCnt2, newTimerCnt2, Timer2Adjust, Timer2C0Snapshot;
unsigned int C0divider, C0multiplier, C0ratio;

/* To put CPU in ASYNC mode and change CPU clock speed */
void BcmPwrMngtSetASCR(unsigned int freq_div)
{
   register unsigned int cp0_ascr_asc;
   register unsigned int temp;
   volatile register unsigned int * clk_alignment_reg = (unsigned int *) CLK_ALIGNMENT_REG;

   // A/ SYNC instruction // Step A, SYNC instruction
   asm("sync" : : );

   // B/ CMT mips : set cp0 reg 22 sel 5 bits [30:28] to 001 (RATIO_ONE_ASYNC)
   asm("mfc0 %0,$22,5" : "=d"(cp0_ascr_asc) :);
   cp0_ascr_asc = ( cp0_ascr_asc & ~MASK_ASCR) | (RATIO_ONE_ASYNC << MASK_ASCR_SHFT);
   asm("mtc0 %0,$22,5" : : "d" (cp0_ascr_asc));

   // // These 3 steps (C,D and E) are needed to work around limitations on clock alignment logics [...]
   // C/ read from 0xff410040   ( make sure you set this to volatile first)
   temp = *clk_alignment_reg;    
    
   // D/ save bit[14:8] to some register, then zero out bit [14:8], write back to same address.
   keepme = temp | KEEPME_MASK;
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96368)
   temp &= ~KEEPME_MASK;
   *clk_alignment_reg = temp;
#endif

   // E/ SYNC instruction   // Step E SYNC instruction  
   asm("sync" : : );

   // F/ change to 1/2, or 1/4, or 1/8 by setting cp0 sel 5 bits[30:28] (sel 4 bits[24:22] for single core mips)  to 011, 101, or 111 respectively
   // Step F change to 1/2, or 1/4, or 1/8 by setting cp0 bits[30:28]
   asm("mfc0 %0,$22,5" : "=d"(temp) :);
   temp = ( temp & ~MASK_ASCR) | (freq_div << MASK_ASCR_SHFT);
   asm("mtc0 %0,$22,5" : : "d" (temp));

   // Step G/ 16 nops // Was 32 nops
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : ); 
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );

   return;
} /* BcmPwrMngtSetASCR */
EXPORT_SYMBOL(BcmPwrMngtSetASCR);


/* To put CPU in SYNC mode and change CPU clock speed to 1:1 ratio */
void BcmPwrMngtSetSCR(void)
{
   register unsigned int cp0_ascr_asc;
   register unsigned int temp;
   volatile register unsigned int * clk_alignment_reg = (unsigned int *) CLK_ALIGNMENT_REG;
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96368)
   unsigned int i;
#endif

   // It is important to go back to divide by 1 async mode first, dont jump directly from divided clock back to SYNC mode.
   // A/ set cp0 reg 22 sel 5 bits[30:28]  (sel 4 bits[24:22] for single core mips)  to 001
   asm("mfc0 %0,$22,5" : "=d"(cp0_ascr_asc) :);
   cp0_ascr_asc = ( cp0_ascr_asc & ~MASK_ASCR) | (RATIO_ONE_ASYNC << MASK_ASCR_SHFT);
   asm("mtc0 %0,$22,5" : : "d" (cp0_ascr_asc));

   // B/ 16 nops // Was 32 nops (wait a while to make sure clk is back to full speed)
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : ); 
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );
   asm("nop" : : ); asm("nop" : : );

   // C/ SYNC instruction
   asm("sync" : : );

   // This step is needed to work around limitations on clock alignment logics for chips from BCM3368 and before BCM6816.
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96368)
   // D/ check for falling edge clock alignment. 
   //     - When bit[22:16] of 0xff410040 is sequence of 0's followed by 1's, then need to write "1" to bit[30],  eg [0011111]
   temp = (*clk_alignment_reg & 0x007F0000) >> 16;
   if (temp == 0x3F || temp == 0x1F || temp == 0x0F || temp == 0x07 || temp == 0x03 || temp == 0x01) {
      *clk_alignment_reg = (*clk_alignment_reg | 0x40000000);
   }
#endif  

   // E/ set bit[14:8] to be 6'b1000001
   *clk_alignment_reg = (*clk_alignment_reg & ~KEEPME_MASK) | 0x4100;

   // F/ repeat
   //  until caught rising edge
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96368)
   i = 30;
#endif

   while (1) {
      //  a/ sread bit[22:16] to check rising edge:
      //   - When bit[22:16] of register 0xff410040 shows sequence of 1's (from bit[22]) followed by 0's (to bit[16]) means good alignment. 
      //    eg [1100000] or [1111000]
      temp = (*clk_alignment_reg & 0x007F0000) >> 16;
      if (temp == 0x40 || temp == 0x60 || temp == 0x70 || temp == 0x78 || temp == 0x7C || temp == 0x7E) {
        break;
      }

#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96368)
      //  b/ every 30 loops, check falling edge(same as step D above), write "1" to bit[30] if falling edge detected
      if (--i == 0) {
         i = 30;
         temp = (*clk_alignment_reg & 0x007F0000) >> 16;
         if (temp == 0x3F || temp == 0x1F || temp == 0x0F || temp == 0x07 || temp == 0x03 || temp == 0x01) {
            *clk_alignment_reg = (*clk_alignment_reg | 0x40000000);
         }
      }
#endif  
   }

   // G/ restore the saved value of bit[14:8] of 0xff410040 back to the register.
   *clk_alignment_reg = (*clk_alignment_reg & ~KEEPME_MASK) | (keepme & KEEPME_MASK);
    
   // H/ set cp0 reg 22 sel 5 bits[30:28]  (sel 4 bits[24:22] for single core mips)  to 000
   asm("mfc0 %0,$22,5" : "=d"(cp0_ascr_asc) :);
   cp0_ascr_asc = ( cp0_ascr_asc & ~MASK_ASCR);
   asm("mtc0 %0,$22,5" : : "d" (cp0_ascr_asc));

   // I/ SYNC instruction 
   asm("sync" : : );

   return;
} /* BcmPwrMngtSetSCR */
EXPORT_SYMBOL(BcmPwrMngtSetSCR);


void BcmPwrMngtSetAutoClkDivide(unsigned int enable)
{
   clock_divide_enabled = enable;
}
EXPORT_SYMBOL(BcmPwrMngtSetAutoClkDivide);


int BcmPwrMngtGetAutoClkDivide(void)
{
   return (clock_divide_enabled);
}
EXPORT_SYMBOL(BcmPwrMngtGetAutoClkDivide);


void BcmPwrMngtSetDRAMSelfRefresh(unsigned int enable)
{
   self_refresh_enabled = enable;
}
EXPORT_SYMBOL(BcmPwrMngtSetDRAMSelfRefresh);


int BcmPwrMngtGetDRAMSelfRefresh(void)
{
   return (self_refresh_enabled);
}
EXPORT_SYMBOL(BcmPwrMngtGetDRAMSelfRefresh);


// Determine if cpu is busy by checking the number of times we entered the wait
// state in the last milisecond. If we entered the wait state only once or
// twice, then the processor is very likely not busy and we can afford to slow
// it down while on wait state. Otherwise, we don't slow down the processor
// while on wait state in order to avoid affecting the time it takes to
// process interrupts
void BcmPwrMngtCheckWaitCount (void)
{
    if (clock_divide_enabled) {
       if (wait_count > 0 && wait_count < 3) {
          clock_divide_low_power = 1;
       }
       else {
          clock_divide_low_power = 0;
       }
       wait_count = 0;
    }
}

// When entering wait state, consider reducing the MIPS clock speed.
// Clock speed is reduced if it has been determined that the cpu was
// mostly idle in the previous milisecond. Clock speed is reduced only
// once per 1 milisecond interval.
void BcmPwrMngtReduceCpuSpeed (void)
{
    // Slow down the clock when entering wait instruction
    // only if the cpu is not busy
    if (clock_divide_low_power) {
        if (wait_count == 0) {
            BcmPwrMngtSetASCR(RATIO_ONE_EIGHTH);
            clock_divide_active = 1;
        }
    }
    wait_count++;
}

// Full MIPS clock speed is resumed on the first interrupt following
// the wait instruction. If the clock speed was reduced, the MIPS
// C0 counter was also slowed down and its value needs to be readjusted.
// The adjustments are done based on a reliable timer from the peripheral
// block, timer2. The adjustments are such that C0 will never drift
// but will see minor jitter.
void BcmPwrMngtResumeFullSpeed (void)
{
    unsigned int mult, rem, new;

    if (clock_divide_active) {
       BcmPwrMngtSetSCR();
    }

    // Check for TimerCnt2 rollover
    newTimerCnt2 = TIMER->TimerCnt2 & 0x3fffffff;
    if (newTimerCnt2 < prevTimerCnt2) {
       Timer2Adjust += mips_hpt_frequency*10;
    }

    // fix the C0 counter because it slowed down while on wait state
    if (clock_divide_active) {
       mult = newTimerCnt2/C0divider;
       rem  = newTimerCnt2%C0divider;
       new  = mult*C0multiplier + ((rem*C0ratio)>>10);
       write_c0_count(Timer2Adjust + Timer2C0Snapshot + new);
       clock_divide_active = 0;
    }
    prevTimerCnt2 = newTimerCnt2;

}


// These numbers can be precomputed. The values are chosen such that the
// calculations will never overflow as long as the MIPS frequency never
// exceeds 850 MHz (hence mips_hpt_frequency must not exceed 425 MHz)
void BcmPwrMngtInitC0Speed (void)
{
    if (mips_hpt_frequency > 425000000) {
       printk("\n\nWarning!!! CPU frequency exceeds limits to support" \
          " Clock Divider feature for Power Management\n");
    }
    C0divider = 50000000/128;
    C0multiplier = mips_hpt_frequency/128;
    C0ratio = ((mips_hpt_frequency/1000000)<<10)/50;
}

