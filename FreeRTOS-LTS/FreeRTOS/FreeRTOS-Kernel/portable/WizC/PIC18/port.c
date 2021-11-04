/*
 * FreeRTOS Kernel V10.4.3 LTS Patch 1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 * 1 tab == 4 spaces!
 */

/*
Changes from V3.2.1
	+ CallReturn Depth increased from 8 to 10 levels to accomodate wizC/fedC V12.

Changes from V3.2.0
	+ TBLPTRU is now initialised to zero during the initial stack creation of a new task. This solves
	an error on devices with more than 64kB ROM.

Changes from V3.0.0
	+ ucCriticalNesting is now initialised to 0x7F to prevent interrupts from being
          handled before the scheduler is started.

Changes from V3.0.1
*/

/* Scheduler include files. */
#include <FreeRTOS.h>
#include <task.h>

#include <malloc.h>

/*---------------------------------------------------------------------------
 * Implementation of functions defined in portable.h for the WizC PIC18 port.
 *---------------------------------------------------------------------------*/

/*
 * We require the address of the pxCurrentTCB variable, but don't want to
 * know any details of its type.
 */
typedef void TCB_t;
extern volatile TCB_t * volatile pxCurrentTCB;

/*
 * Define minimal-stack constants
 * -----
 * FSR's:
 *		STATUS, WREG, BSR, PRODH, PRODL, FSR0H, FSR0L,
 *		FSR1H, FSR1L,TABLAT, (TBLPTRU), TBLPTRH, TBLPTRL,
 *		(PCLATU), PCLATH
 *		sfr's within parenthesis only on devices > 64kB
 * -----
 * Call/Return stack:
 *		 2 bytes per entry on devices <= 64kB
 *		 3 bytes per entry on devices >  64kB
 * -----
 * Other bytes:
 *		 2 bytes: FunctionParameter for initial taskcode
 *		 1 byte : Number of entries on call/return stack
 *		 1 byte : ucCriticalNesting
 *		16 bytes: Free space on stack
 */
#if _ROMSIZE > 0x8000
	#define portSTACK_FSR_BYTES				( 15 )
	#define portSTACK_CALLRETURN_ENTRY_SIZE	(  3 )
#else
	#define portSTACK_FSR_BYTES				( 13 )
	#define portSTACK_CALLRETURN_ENTRY_SIZE	(  2 )
#endif

#define portSTACK_MINIMAL_CALLRETURN_DEPTH	( 10 )
#define portSTACK_OTHER_BYTES				( 20 )

uint16_t usCalcMinStackSize		= 0;

/*-----------------------------------------------------------*/

/*
 * We initialise ucCriticalNesting to the middle value an
 * uint8_t can contain. This way portENTER_CRITICAL()
 * and portEXIT_CRITICAL() can be called without interrupts
 * being enabled before the scheduler starts.
 */
register uint8_t ucCriticalNesting = 0x7F;

/*-----------------------------------------------------------*/

/*
 * Initialise the stack of a new task.
 * See portSAVE_CONTEXT macro for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
uint8_t ucScratch;
	/*
	 * Get the size of the RAMarea in page 0 used by the compiler
	 * We do this here already to avoid W-register conflicts.
	 */
	_Pragma("asm")
		movlw	OVERHEADPAGE0-LOCOPTSIZE+MAXLOCOPTSIZE
		movwf	PRODL,ACCESS		; PRODL is used as temp register
	_Pragma("asmend")
	ucScratch = PRODL;

	/*
	 * Place a few bytes of known values on the bottom of the stack.
	 * This is just useful for debugging.
	 */
//	*pxTopOfStack--	= 0x11;
//	*pxTopOfStack-- = 0x22;
//	*pxTopOfStack-- = 0x33;

	/*
	 * Simulate how the stack would look after a call to vPortYield()
	 * generated by the compiler.
	 */

	/*
	 * First store the function parameters.  This is where the task expects
	 * to find them when it starts running.
	 */
	*pxTopOfStack-- = ( StackType_t ) ( (( uint16_t ) pvParameters >> 8) & 0x00ff );
	*pxTopOfStack-- = ( StackType_t ) (  ( uint16_t ) pvParameters       & 0x00ff );

	/*
	 * Next are all the registers that form part of the task context.
	 */
	*pxTopOfStack-- = ( StackType_t ) 0x11; /* STATUS. */
	*pxTopOfStack-- = ( StackType_t ) 0x22; /* WREG. */
	*pxTopOfStack-- = ( StackType_t ) 0x33; /* BSR. */
	*pxTopOfStack-- = ( StackType_t ) 0x44; /* PRODH. */
	*pxTopOfStack-- = ( StackType_t ) 0x55; /* PRODL. */
	*pxTopOfStack-- = ( StackType_t ) 0x66; /* FSR0H. */
	*pxTopOfStack-- = ( StackType_t ) 0x77; /* FSR0L. */
	*pxTopOfStack-- = ( StackType_t ) 0x88; /* FSR1H. */
	*pxTopOfStack-- = ( StackType_t ) 0x99; /* FSR1L. */
	*pxTopOfStack-- = ( StackType_t ) 0xAA; /* TABLAT. */
#if _ROMSIZE > 0x8000
	*pxTopOfStack-- = ( StackType_t ) 0x00; /* TBLPTRU. */
#endif
	*pxTopOfStack-- = ( StackType_t ) 0xCC; /* TBLPTRH. */
	*pxTopOfStack-- = ( StackType_t ) 0xDD; /* TBLPTRL. */
#if _ROMSIZE > 0x8000
	*pxTopOfStack-- = ( StackType_t ) 0xEE; /* PCLATU. */
#endif
	*pxTopOfStack-- = ( StackType_t ) 0xFF; /* PCLATH. */

	/*
	 * Next the compiler's scratchspace.
	 */
	while(ucScratch-- > 0)
	{
		*pxTopOfStack-- = ( StackType_t ) 0;
	}

	/*
	 * The only function return address so far is the address of the task entry.
	 * The order is TOSU/TOSH/TOSL. For devices > 64kB, TOSU is put on the
	 * stack, too. TOSU is always written as zero here because wizC does not allow
	 * functionpointers to point above 64kB in ROM.
	 */
#if _ROMSIZE > 0x8000
	*pxTopOfStack-- = ( StackType_t ) 0;
#endif
	*pxTopOfStack-- = ( StackType_t ) ( ( ( uint16_t ) pxCode >> 8 ) & 0x00ff );
	*pxTopOfStack-- = ( StackType_t ) ( (   uint16_t ) pxCode        & 0x00ff );

	/*
	 * Store the number of return addresses on the hardware stack.
	 * So far only the address of the task entry point.
	 */
	*pxTopOfStack-- = ( StackType_t ) 1;

	/*
	 * The code generated by wizC does not maintain separate
	 * stack and frame pointers. Therefore the portENTER_CRITICAL macro cannot
	 * use the stack as per other ports.  Instead a variable is used to keep
	 * track of the critical section nesting.  This variable has to be stored
	 * as part of the task context and is initially set to zero.
	 */
	*pxTopOfStack-- = ( StackType_t ) portNO_CRITICAL_SECTION_NESTING;

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

uint16_t usPortCALCULATE_MINIMAL_STACK_SIZE( void )
{
	/*
	 * Fetch the size of compiler's scratchspace.
	 */
	_Pragma("asm")
		movlw	OVERHEADPAGE0-LOCOPTSIZE+MAXLOCOPTSIZE
		movlb	usCalcMinStackSize>>8
		movwf	usCalcMinStackSize,BANKED
	_Pragma("asmend")

	/*
	 * Add minimum needed stackspace
	 */
	usCalcMinStackSize	+=	( portSTACK_FSR_BYTES )
		+	( portSTACK_MINIMAL_CALLRETURN_DEPTH * portSTACK_CALLRETURN_ENTRY_SIZE )
		+	( portSTACK_OTHER_BYTES );

	return(usCalcMinStackSize);
}

/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
	extern void portSetupTick( void );

	/*
	 * Setup a timer for the tick ISR for the preemptive scheduler.
	 */
	portSetupTick();

	/*
	 * Restore the context of the first task to run.
	 */
	portRESTORE_CONTEXT();

	/*
	 * This point should never be reached during execution.
	 */
	return pdTRUE;
}

/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/*
	 * It is unlikely that the scheduler for the PIC port will get stopped
	 * once running. When called a reset is done which is probably the
	 * most valid action.
	 */
	_Pragma(asmline reset);
}

/*-----------------------------------------------------------*/

/*
 * Manual context switch.  This is similar to the tick context switch,
 * but does not increment the tick count.  It must be identical to the
 * tick context switch in how it stores the stack of a task.
 */
void vPortYield( void )
{
	/*
	 * Save the context of the current task.
	 */
	portSAVE_CONTEXT( portINTERRUPTS_UNCHANGED );

	/*
	 * Switch to the highest priority task that is ready to run.
	 */
	vTaskSwitchContext();

	/*
	 * Start executing the task we have just switched to.
	 */
	portRESTORE_CONTEXT();
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	void *pvPortMalloc( uint16_t usWantedSize )
	{
	void *pvReturn;

		vTaskSuspendAll();
		{
			pvReturn = malloc( ( malloc_t ) usWantedSize );
		}
		xTaskResumeAll();

		return pvReturn;
	}

#endif /* configSUPPORT_STATIC_ALLOCATION */

/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

	void vPortFree( void *pv )
	{
		if( pv )
		{
			vTaskSuspendAll();
			{
				free( pv );
			}
			xTaskResumeAll();
		}
	}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION */