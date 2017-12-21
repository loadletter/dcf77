/*** FILEHEADER ****************************************************************
 *
 *   FILENAME:    dcf77.h
 *   DATE:        21.11.2004
 *   AUTHOR:       (c) Christian Stadler
 *
 *   DESCRIPTION: Driver to decode the DCF77 signal.
 *
 *              DCF77 Signal Format:
 *              ====================
 *               0. Minutenbeginn (immer LOW)
 *               1. - 14. Reserviert, keine Bedeutung
 *              15. Reserveantenne aktiv
 *              16. Umstellung von Sommer- auf Winterzeit, oder umgekehrt
 *              17. Sommerzeit aktiv
 *              18. Winterzeit aktiv
 *              19. Ankündigung Schaltsekunde
 *              20. Zeitbeginn
 *              21. - 27. Minute 1, 2, 4, 8, 10, 20, 40
 *              28. Prüfbit Minute
 *              29. - 34. Stunde 1, 2, 4, 8, 10, 20
 *              35. Prüfbit Stunde
 *              36. - 41. Tag 1, 2, 4, 8, 10, 20
 *              42. - 44. Wochentag 1, 2, 4
 *              45. - 49. Monat 1, 2, 4, 8, 10
 *              50. - 57. Jahr 1, 2, 4, 8, 10, 20, 40, 80
 *              58. Prüfbit Datum
 *              59. fehlt zur Erkennung des Minutenanfangs
 *
 ******************************************************************************/

/*** HISTORY OF CHANGE *********************************************************
 *
 *   $Log: /pic/_drv/dcf77.h $
 * 
 * 13    21.12.17 22:20 loadletter
 * - add sanity checks to dcf77_newdata()
 * 
 * 12    8.12.17 19:00 loadletter
 * - split into .c/.h
 * - make get functions return decimal instead of weird BCD integers
 * - rewrite parity calculation
 * 
 * 11    4.12.10 22:52 Stadler
 * - fixed wrong "day of the week" masking
 * 
 * 10    23.11.10 19:56 Stadler
 * - added dcf77_get_dow() API
 * - added parity check for date
 * 
 * 8     21.11.10 20:35 Stadler
 * - improved sync detection
 * - added check for 0 in bit 0 for data valid check
 * 
 * 7     20.11.10 16:32 Stadler
 * - improved sync detection
 * - added dcf77_newdata() API
 * 
 * 6     20.11.10 14:59 Stadler
 * - improved dcf77_datavalid (added check for valid start of time
 * information)
 * - added check for proper bit idle time
 * - fixed problem with sample counter reset
 * 
 * 5     19.11.10 17:52 Stadler
 * - added parity check for hour and minute
 * 
 * 4     10.02.10 21:16 Stadler
 * 1) Improved 1ms counter handling for DCF77 signal sampling.
 * 2) Fixed illegal memory access if start of minute is not found.
 * 
 * 3     9.02.10 19:47 Stadler
 * Renamed global variables.
 * 
 * 2     8.02.10 20:26 Stadler
 * Implemented DCF77 signal debouncing
 * 
 * 1     26.02.05 18:27 Stadler
 * Initial creation of the DCF77 driver.
 *
 ******************************************************************************/

#include "dcf77.h"

#define bit_test(D,i) ((D) & (0x01 << (i))) 
#define bcd_to_dec(D) ((((D) & 0xF0) >> 4) * 10 + ((D) & 0x0F))
#define check_parity(A,B) ((!(A)) == (!(B)) )

#ifndef DCF77_INPUT
#error DCF77 digital input pin has to be define!
#endif

/* --- global variables ---------------------------------------------------- */
static uint8  dcf77_pinold_b = FALSE;          /* debounced DCF77 input pin state */
static uint8  dcf77_newdata_b = FALSE;         /* new data flag */
static uint8  dcf77_bitpos_u8 = 0;             /* current bit position of DCF77 signal */
static uint16 dcf77_cntms_u16 = 0;             /* 1ms counter for DCF77 signal sampling */
static uint8  dfc77_datatmp_au8[8];            /* data which is currently sampled */
static uint8  dcf77_data_au8[8];               /* valid DCF77 data */
static uint8  dcf77_sync_u8 = DCF77_SYNC_WAIT; /* DCF77 sync state */


/******************************************************************************
 * dcf77_get_status
 *****************************************************************************/
uint8 dcf77_get_status(void)
{
	return dcf77_sync_u8;
}


/******************************************************************************
 * dcf77_get_sync
 *****************************************************************************/
uint8 dcf77_get_sync(void)
{
	return (dcf77_sync_u8 == DCF77_SYNC_OK);
}


/******************************************************************************
 * dcf77_get_min (bitpos 21-27)
 *****************************************************************************/
uint8 dcf77_get_min(void)
{
	uint8 data1;
	uint8 data2;

	data1 = ((dcf77_data_au8[2] & 0xE0) >> 5);
	data2 = ((dcf77_data_au8[3] & 0x0F) << 3);

	data1 = data1 | data2;

	return bcd_to_dec(data1);
}


/******************************************************************************
 * dcf77_get_hrs (bitpos 29-34)
 *****************************************************************************/
uint8 dcf77_get_hrs(void)
{
	uint8 data1;
	uint8 data2;

	data1 = ((dcf77_data_au8[3] & 0xE0) >> 5);
	data2 = ((dcf77_data_au8[4] & 0x07) << 3);

	data1 = data1 | data2;

	return bcd_to_dec(data1);
}


/******************************************************************************
 * dcf77_get_day (bitpos 36-41)
 *****************************************************************************/
uint8 dcf77_get_day(void)
{
	uint8 data1;
	uint8 data2;

	data1 = ((dcf77_data_au8[4] & 0xF0) >> 4);
	data2 = ((dcf77_data_au8[5] & 0x03) << 4);

	data1 = data1 | data2;

	return bcd_to_dec(data1);
}


/******************************************************************************
 * dcf77_get_dow (42 - 44)
 *****************************************************************************/
uint8 dcf77_get_dow(void)
{
	uint8 data;

	data = ((dcf77_data_au8[5] >> 2) & 0x7);
	/* bcd to dec not needed here */
	return (data);
}


/******************************************************************************
 * dcf77_get_month (45 - 49)
 *****************************************************************************/
uint8 dcf77_get_month(void)
{
	uint8 data1;
	uint8 data2;

	data1 = ((dcf77_data_au8[5] & 0xE0) >> 5);
	data2 = ((dcf77_data_au8[6] & 0x03) << 3);

	data1 = data1 | data2;

	return bcd_to_dec(data1);
}


/******************************************************************************
 * dcf77_get_year (50 - 57)
 *****************************************************************************/
uint16 dcf77_get_year(void)
{
	uint16 year;
	uint8 data1;
	uint8 data2;

	data1 = ((dcf77_data_au8[6] & 0xFC) >> 2);
	data2 = ((dcf77_data_au8[7] & 0x03) << 6);

	/* add century */
	year = bcd_to_dec(data1 | data2) + 2000;

	return (year);
}

/* byte parity calculation */
uint8 calc_parity(uint8 n)
{
	uint8 p;
	p = n << 4;
	p ^= n;
	n = p << 2;
	p ^= n;
	p += 0X40;
	return (p & 0X80);
}

/* multibyte parity calculation (check if end result is even/odd) */
uint8 calc_parity_part(uint8 n, uint8 prev)
{
	uint8 p = prev;
	 if(n & 0x1) 
		 p++; 
	 if(n & 0x2) 
		 p++; 
	 if(n & 0x4) 
		 p++; 
	 if(n & 0x8) 
		 p++; 
	 if(n & 0x10) 
		 p++; 
	 if(n & 0x20) 
		 p++; 
	 if(n & 0x40) 
		 p++; 
	 if(n & 0x80) 
		 p++; 
	 
	 return p;
}

/******************************************************************************
 * dcf77_newdata
 *****************************************************************************/
uint8 dcf77_newdata(void)
{
	uint8 valid;
	uint8 parity;
	uint8 data;
	
	uint8 data1;
	uint8 data2;

	/* if sync is OK and */
	/* start of new minute is 0 (start of minute is always 1) */
	/* start of time data is 1 (Zeitbeginn always 1) */
	if ( (dcf77_sync_u8 == DCF77_SYNC_OK) &&
		 (!bit_test(dcf77_data_au8[0], 0)) &&
		 (bit_test(dcf77_data_au8[2], 4)) )
	{
		/* check for valid minute */
		data1 = ((dcf77_data_au8[2] & 0xE0) >> 5);
		data2 = ((dcf77_data_au8[3] & 0x0F) << 3);
		data = data1 | data2;
		parity = calc_parity(data);
		valid = check_parity(parity, bit_test(dcf77_data_au8[3], 4));
		if (data > 0x59)
		{
			valid = FALSE;
		}	
		if (!valid)
		{
			goto invalidparity;
		}
		
		/* check for valid hour */
		data1 = ((dcf77_data_au8[3] & 0xE0) >> 5);
		data2 = ((dcf77_data_au8[4] & 0x07) << 3);
		data = data1 | data2;
		parity = calc_parity(data);
		valid = check_parity(parity, bit_test(dcf77_data_au8[4], 3));
		if (data > 0x23)
		{
			valid = FALSE;
		}	
		if (!valid)
		{
			goto invalidparity;
		}
		
		/* check for valid date */
		parity = 0;
		//day
		data1 = ((dcf77_data_au8[4] & 0xF0) >> 4);
		data2 = ((dcf77_data_au8[5] & 0x03) << 4);
		data = data1 | data2;
		if (data > 0x31)
		{
			valid = FALSE;
			goto invalidparity;
		}
		parity = calc_parity_part(data, parity);
		//dow
		data = dcf77_get_dow();
		if (data > 0x7)
		{
			valid = FALSE;
			goto invalidparity;
		}
		parity = calc_parity_part(data, parity);
		//month
		data1 = ((dcf77_data_au8[5] & 0xE0) >> 5);
   		data2 = ((dcf77_data_au8[6] & 0x03) << 3);
		data = data1 | data2;
		if (data > 0x12)
		{
			valid = FALSE;
			goto invalidparity;
		}
		parity = calc_parity_part(data, parity);
		//year
		data1 = ((dcf77_data_au8[6] & 0xFC) >> 2);
		data2 = ((dcf77_data_au8[7] & 0x03) << 6);
		parity = calc_parity_part(data1 | data2, parity);
		valid = check_parity(parity & 1, bit_test(dcf77_data_au8[7], 2));
	}
	else
	{
		valid = FALSE;
	}
	invalidparity:
	
	/* if data is valid and new data available */
	valid = (valid && dcf77_newdata_b) ? TRUE : FALSE;

	/* reset new data flag */
	dcf77_newdata_b = FALSE;

	return (valid);
}

/******************************************************************************
 * dcf77_1ms_task (needs to be called exactly every 1ms, e.g. in timer
 * timer interrupt service routine)
 *****************************************************************************/
void dcf77_1ms_task(void)
{
	static uint8 debcnt = 0;
	static uint8 pindeb_b = 0;
	uint8 index;
	uint8 data_b;
	uint8 pin_b;

	/* increment 1ms counter */
	dcf77_cntms_u16 ++;

	/* get current state of data pin */
	pin_b = DCF77_INPUT;


	/* --- debounce pin state ---------------------------------------------- */
	if (pin_b == pindeb_b)
	{
		debcnt = 0;
	}
	else
	{
		debcnt ++;
	  
		if (debcnt >= DCF77_DEBOUNCE_TIME)
		{
			pindeb_b = pin_b;
			debcnt = 0;
		}
	}


	/* --- decode DFC77 signal --------------------------------------------- */

	/* check if state of pin has changed */
	if (dcf77_pinold_b != pindeb_b)
	{
		/* falling edge of signal */
		if (pindeb_b == FALSE)
		{
			/* check for start */
			if ( (dcf77_cntms_u16 > DCF77_SIG_START_MIN) && (dcf77_cntms_u16 < DCF77_SIG_START_MAX) )
			{
				/* if we where waiting for 1st sync */
				if (dcf77_sync_u8 == DCF77_SYNC_WAIT)
				{
					dcf77_sync_u8 = DCF77_SYNC_FIRST;
				}
				/* if 1 valid cycle has been received */
				else if ( (dcf77_sync_u8 >= DCF77_SYNC_FIRST) && (dcf77_bitpos_u8 == 59) )
				{
					dcf77_sync_u8 = DCF77_SYNC_OK;
				
					//memcpy(dcf77_data_au8, dfc77_datatmp_au8, sizeof(dfc77_datatmp_au8));
					for (index=0;index<sizeof(dcf77_data_au8);index++)
					{
						dcf77_data_au8[index] = dfc77_datatmp_au8[index];
					} 
					
					dcf77_newdata_b = TRUE;
				}
				/* valid start condition but wrong number of bits deteced during last cycle */
				else
				{
					dcf77_sync_u8 = DCF77_SYNC_WAIT;
				}
				dcf77_bitpos_u8 = 0;
			}
			/* check for invalid idle time (FIXME: after high wait 800ms idle, after low 900ms idle) */
			else if ( (dcf77_cntms_u16 < DCF77_BIT_IDLE_MIN) || (dcf77_cntms_u16 > DCF77_BIT_IDLE_MAX) )
			{
				dcf77_sync_u8 = DCF77_SYNC_WAIT;
			}
			else
			{
				/* bit time OK, keep state */
			}
		}
		/* rising edge of signal */
		else
		{
			/* check for logic "0" */
			if ( (dcf77_cntms_u16 > DCF77_BIT_LOW_MIN) && (dcf77_cntms_u16 < DCF77_BIT_LOW_MAX) )
			{
				data_b = 0;
			}
			/* check for logic "1" */
			else if ( (dcf77_cntms_u16 > DCF77_BIT_HIGH_MIN) && (dcf77_cntms_u16 < DCF77_BIT_HIGH_MAX) )
			{
				data_b = 1;
			}
			/* wrong timing detected */
			else
			{
				dcf77_sync_u8 = DCF77_SYNC_WAIT;
			}

			/* if synchronized and current bit position is valid save bit state */
			if ( (dcf77_sync_u8 >= DCF77_SYNC_FIRST) && (dcf77_bitpos_u8 <= 59) )
			{
				index = dcf77_bitpos_u8 / 8;

				if (data_b)
				{
					dfc77_datatmp_au8[index] |= (1 << (dcf77_bitpos_u8 % 8));
				}
				else
				{
					dfc77_datatmp_au8[index] &= ~(1 << (dcf77_bitpos_u8 % 8));
				}

				dcf77_bitpos_u8 ++;
			}
			else
			{
				/* if bit position is greater than 59 then there is something wrong */
				dcf77_sync_u8 = DCF77_SYNC_WAIT;
			}
		
		}
		  
		/* reset ms counter */
		dcf77_cntms_u16 = 0;
	}

	/* save state of data pin */
	dcf77_pinold_b = pindeb_b;
}
