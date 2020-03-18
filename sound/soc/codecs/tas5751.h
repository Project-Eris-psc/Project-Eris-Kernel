/*
 * TAS571x amplifier audio driver
 *
 * Copyright (C) 2015 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _TAS5751_H
#define _TAS5751_H

/* device registers */

typedef enum _eTas5711RegAddr
{
	eClkCtl = 0x00,
	eDevID,
	eErrStat,
	eSysCtl1,
	eSerialData,
	eSysCtl2,
	eSoftMute,
	eMasterVol,

	eCh1Vol,
	eCh2Vol,
	eFineMasterVol,
	eVolCfg = 0x0e,

	eModulationLimit = 0x10,
	eICDelayCh1,
	eICDelayCh2,
	eICDelayCh3,
	eICDelayCh4,

	sPWMStart = 0x18,
	ePWMSDG = 0x19,

	eStartStopPeriod = 0x1A,
	eOscillatorTrim,
	eBKNDErr,

	eInputMux = 0x20,

	ePWMMux = 0x25,

	eCh1Bq0 = 0x26,
	eCh1Bq1,
	eCh1Bq2,
	eCh1Bq3,
	eCh1Bq4,
	eCh1Bq5,
	eCh1Bq6,
	eCh1Bq7,
	eCh1Bq8,
	eCh1Bq9,
   
	eCh2Bq0 = 0x30,
	eCh2Bq1,
	eCh2Bq2,
	eCh2Bq3,
	eCh2Bq4,
	eCh2Bq5,
	eCh2Bq6,
	eCh2Bq7,
	eCh2Bq8,
	eCh2Bq9,
	
	eAGL1SoftFt= 0x3B,
	eAGL1AttkRelseRate,

	eAGL2SoftFt= 0x3E,
	eAGL2AttkRelseRate,

	eAGL1AttkThd = 0x40,
	eAGL3AttkThd,
	eAGL3AttkRelseRate,
	eAGL2AttkThd,
	eAGL4AttkThd,
	eAGL4AttkRelseRate,
	
	eAGLCtrl = 0x46,

	eAGL3SoftFt,
	eAGL4SoftFt,

	PWMSwRateCtrl = 0x4f,
	BankSwCtrl,
	
	eCh1OutPutMixer = 0x51,
	eCh2OutPutMixer = 0X52,

	OutPurPostScale = 0x56,
	OutPurPreScale,
	
	eSubChBQ0 = 0x5A,
	eSubChBQ1,
}eTas5711RegAddr;

#define MAINVOL_OFFSET 0x68
#define VOLTABLEMAX 41 
const unsigned short bMainVolTable[VOLTABLEMAX]=
   {
	   
	   0x03FF , 		   // 0 	   mute 			
	   0x2E8  , 		   // 1 	   -69db			
	   0x2B8  , 	 // 2		 -63db			  
	   0x270  , 	 // 3		 -54db			  
	   0x228  , 	 // 4		 -45db			  
	   0x1F8  , 	 // 5		 -39db			  
	   0x1C8  , 	 // 6		 -33db			  
	   0x1A8  , 	 // 7		 -29db			  
	   0x190  , 	 // 8		 -26db			  
	   0x180  , 	 // 9		 -24db			  
	   0x178  , 	 // 10		 -23db			  
	   0x170  , 	 // 11		 -22db			  
	   0x168  , 	 // 12		 -21db			  
	   0x160  , 	 // 13		 -20db			  
	   0x158  , 	 // 14		 -19db			  
	   0x150  , 	 // 15		 -18db			  
	   0x148  , 	 // 16		 -17db			  
	   0x140  , 	 // 17		 -16db			  
	   0x138  , 	 // 18		 -15db			  
	   0x130  , 	 // 19		 -14db			  
	   0x128  , 	 // 20		 -13db			  
	   0x120  , 	 // 21		 -12db			  
	   0x118  , 	 // 22		 -11db			  
	   0x111  , 	 // 23		 -10.125 db 	  
	   0x109  , 	 // 24		 -9.125  db 	  
	   0x100  , 	 // 25		 -8 db			  
	   0xFA   , 	 // 26		 -7.25db		  
	   0xF6   , 	 // 27		 -6.75db		  
	   0xF2   , 	 // 28		 -6.25db		  
	   0xEE   , 	 // 29		 -5.75db		  
	   0xEA   , 	 // 30		 -5.25db		  
	   0xE6   , 	 // 31		 -4.75db		  
	   0xE2   , 	 // 32		 -4.25db		  
	   0xDE   , 	 // 33		 -3.75db		  
	   0xD9   , 	 // 34		 -3.125db		  
	   0xD3   , 	 // 35		 -2.375db		  
	   0xD0   , 	 // 36		 -2db			  
	   0xCD   , 	 // 37		 -1.625db		  
	   0xC7   , 	 // 38		 -0.875db		  
	   0xC4   , 	 // 39		 -0.5db 		  
	   0xC0   , 	 // 40		 0db			  
		   #if 0
		   0x03FF,				 //  0		   mute 	   
		   0x2A8 ,				 //  1		   -61	DB	   
		   0x288 ,				 //  2		   -57	DB	   
		   0x268 ,				 //  3		   -53	DB	   
		   0x250 ,				 //  4		   -50	DB	   
		   0x238 ,				 //  5		   -47	DB	   
		   0x220 ,				 //  6		   -44	DB	   
		   0x210 ,				 //  7		   -42	DB	   
		   0x200 ,				 //  8		   -40	DB	   
		   0x1F0 ,				 //  9		   -38	DB	   
		   0x1E0 ,				 //  10 	   -36	DB	   
		   0x1D0 ,				 //  11 	   -34	DB	   
		   0x1C0 ,				 //  12 	   -32	DB	   
		   0x1B0 ,				 //  13 	   -30	DB	   
		   0x1A0 ,				 //  14 	   -28	DB	   
		   0x190 ,				 //  15 	   -26	DB	   
		   0x180 ,				 //  16 	   -24	DB	   
		   0x170 ,				 //  17 	   -22	DB	   
		   0x160 ,				 //  18 	   -20	DB	   
		   0x150 ,				 //  19 	   -18	DB	   
		   0x140 ,				 //  20 	   -16	DB	   
		   0x138 ,				 //  21 	   -15	DB	   
		   0x130 ,				 //  22 	   -14	DB	   
		   0x128 ,				 //  23 	   -13	DB	   
		   0x120 ,				 //  24 	   -12	DB	   
		   0x118 ,				 //  25 	   -11	DB	   
		   0x110 ,				 //  26 	   -10	DB	   
		   0x108 ,				 //  27 	   -9	DB	   
		   0x100 ,				 //  28 	   -8	DB	   
		   0xF8  ,				 //  29 	   -7	DB	   
		   0xF0  ,				 //  30 	   -6	DB	   
		   0xE8  ,				 //  31 	   -5	DB	   
		   0xE0  ,				 //  32 	   -4	DB	   
		   0xDC  ,			 //  33 	   -3.5 DB		   
		   0xD8  ,				 //  34 	   -3	DB	   
		   0xD4  ,				 //  35 	   -2.5 DB	   
		   0xD0  ,				 //  36 	   -2	DB	   
		   0xCC  ,				 //  37 	   -1.5 DB	   
		   0xC8  ,				 //  38 	   -1	DB	   
		   0xC4  ,				 //  39 	   -0.5 DB	   
		   0xC0  ,				 //  40 	   0		DB	   
		   #endif
   };


#endif /* _TAS5751_H */

