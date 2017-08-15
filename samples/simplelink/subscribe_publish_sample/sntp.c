//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
//
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

// Standard includes
#include <stdio.h>

// simplelink includes
#include "simplelink.h"

//Free_rtos/ti-rtos includes
#include "osi.h"

// common interface includes
#include "device.h"
#ifndef NOTERM
#include "uart_if.h"
#endif
#include "common.h"

#define TIME2013                3565987200u      /* 113 years + 28 days(leap) */
#define YEAR2013                2013
#define SEC_IN_MIN              60
#define SEC_IN_HOUR             3600
#define SEC_IN_DAY              86400

#define SERVER_RESPONSE_TIMEOUT 10
#define GMT_DIFF_TIME_HRS       0
#define GMT_DIFF_TIME_MINS      0

#define SOCKET_TIMEOUT_VAL 		5000		// Timeout period while waiting for non-blocking socket APIs
#define SOCKET_POLL_INTERVAL	100		// Polling interval for non-blocking socket APIs

#define NUM_NTP_SERVERS			4

unsigned short g_usTimerInts;
SlSecParams_t SecurityParams = {0};

// Tuesday is the 1st day in 2013 - the relative year
const char g_acDaysOfWeek2013[7][3] = {{"Tue"},
                                    {"Wed"},
                                    {"Thu"},
                                    {"Fri"},
                                    {"Sat"},
                                    {"Sun"},
                                    {"Mon"}};

const char g_acMonthOfYear[12][3] = {{"Jan"},
                                  {"Feb"},
                                  {"Mar"},
                                  {"Apr"},
                                  {"May"},
                                  {"Jun"},
                                  {"Jul"},
                                  {"Aug"},
                                  {"Sep"},
                                  {"Oct"},
                                  {"Nov"},
                                  {"Dec"}};

const char g_acNumOfDaysPerMonth[12] = {31, 28, 31, 30, 31, 30,
                                        31, 31, 30, 31, 30, 31};

const char g_acDigits[] = "0123456789";

SlDateTime_t g_time;

struct
{
    unsigned long ulDestinationIP;
    int iSockID;
    unsigned long ulElapsedSec;
    short isGeneralVar;
    unsigned long ulGeneralVar;
    unsigned long ulGeneralVar1;
    char acTimeStore[30];
    char *pcCCPtr;
    unsigned short uisCCLen;
}g_sAppData;

SlSockAddr_t sAddr;
SlSockAddrIn_t sLocalAddr;

// List of NTP servers to try
const char g_acSNTPserver[NUM_NTP_SERVERS][30] = {	"0.pool.ntp.org",
													"1.pool.ntp.org",
													"0.us.pool.ntp.org",
													"1.us.pool.ntp.org"};

unsigned short itoa(short cNum, char *cString);

// Get time from designated NTP server. Return 0 on success, negative on error
long GetSntpTime(unsigned char ucGmtDiffHr, unsigned char ucGmtDiffMins)
{

/*
                            NTP Packet Header:


       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9  0  1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |LI | VN  |Mode |    Stratum    |     Poll      |   Precision    |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          Root  Delay                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                       Root  Dispersion                         |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                     Reference Identifier                       |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                                |
      |                    Reference Timestamp (64)                    |
      |                                                                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                                |
      |                    Originate Timestamp (64)                    |
      |                                                                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                                |
      |                     Receive Timestamp (64)                     |
      |                                                                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                                |
      |                     Transmit Timestamp (64)                    |
      |                                                                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                 Key Identifier (optional) (32)                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                                |
      |                                                                |
      |                 Message Digest (optional) (128)                |
      |                                                                |
      |                                                                |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
    char cDataBuf[48];
    long lRetVal = 0;
    int iAddrSize;
    long temp;

    // Send a query ? to the NTP server to get the NTP time
    memset(cDataBuf, 0, sizeof(cDataBuf));
    cDataBuf[0] = '\x1b';

    sAddr.sa_family = SL_AF_INET;
    sAddr.sa_data[0] = 0x00;
    sAddr.sa_data[1] = 0x7B;    // UDP port number for NTP is 123
    sAddr.sa_data[2] = (char)((g_sAppData.ulDestinationIP>>24)&0xff);
    sAddr.sa_data[3] = (char)((g_sAppData.ulDestinationIP>>16)&0xff);
    sAddr.sa_data[4] = (char)((g_sAppData.ulDestinationIP>>8)&0xff);
    sAddr.sa_data[5] = (char)(g_sAppData.ulDestinationIP&0xff);

    lRetVal = sl_SendTo(g_sAppData.iSockID,
                     cDataBuf,
                     sizeof(cDataBuf), 0,
                     &sAddr, sizeof(sAddr));
    if (lRetVal != sizeof(cDataBuf))
    {
        // could not send SNTP request
        ASSERT_ON_ERROR(-1);
    }

    // Wait to receive the NTP time from the server
    sLocalAddr.sin_family = SL_AF_INET;
    sLocalAddr.sin_port = 0;
    sLocalAddr.sin_addr.s_addr = 0;
    if(g_sAppData.ulElapsedSec == 0)
    {
        lRetVal = sl_Bind(g_sAppData.iSockID,
                (SlSockAddr_t *)&sLocalAddr,
                sizeof(SlSockAddrIn_t));
    }

    // Receive message from server
    iAddrSize = sizeof(SlSockAddrIn_t);
	lRetVal = sl_RecvFrom(g_sAppData.iSockID,
					   cDataBuf, sizeof(cDataBuf), 0,
					   (SlSockAddr_t *)&sLocalAddr,
					   (SlSocklen_t*)&iAddrSize);
    ASSERT_ON_ERROR(lRetVal);

    // Confirm that the MODE is 4 --> server
    if ((cDataBuf[0] & 0x7) != 4)    // expect only server response
    {
         ASSERT_ON_ERROR(-1);  // MODE is not server, abort
    }
    else
    {
        unsigned char iIndex;

        //
        // Getting the data from the Transmit Timestamp (seconds) field
        // This is the time at which the reply departed the
        // server for the client
        //
        g_sAppData.ulElapsedSec = cDataBuf[40];
        g_sAppData.ulElapsedSec <<= 8;
        g_sAppData.ulElapsedSec += cDataBuf[41];
        g_sAppData.ulElapsedSec <<= 8;
        g_sAppData.ulElapsedSec += cDataBuf[42];
        g_sAppData.ulElapsedSec <<= 8;
        g_sAppData.ulElapsedSec += cDataBuf[43];

        // Make relative to 2013 and correct for timezone
        g_sAppData.ulElapsedSec -= TIME2013;						// Make relative from beginning of 2013 instead of 1900
        g_sAppData.ulElapsedSec += (ucGmtDiffHr * SEC_IN_HOUR);		// Correct for timezone hours
        g_sAppData.ulElapsedSec += (ucGmtDiffMins * SEC_IN_MIN);	// Correct for timezone minutes

        //
        // Get year, month and day
        //
        temp = g_sAppData.ulElapsedSec/SEC_IN_DAY;					// Number of days since 2013
        g_time.sl_tm_year = temp / 365;									// Number of years since 2013
        g_time.sl_tm_year += 2013;									// Absolute year A.D.
        temp %= 365;												// Day of the year
        for (iIndex = 0; iIndex < 12; iIndex++)
        {
        	temp -= g_acNumOfDaysPerMonth[iIndex];
            if (temp < 0) break;
        }
        if(iIndex == 12)
        {
            iIndex = 0;
        }
        g_time.sl_tm_mon = iIndex;									// Set month
        g_time.sl_tm_day = temp + g_acNumOfDaysPerMonth[iIndex];	// Correct day of the month

        //
        // Get time
        //
        temp = g_sAppData.ulElapsedSec % SEC_IN_DAY;				// Seconds since 00:00:00 today
        g_time.sl_tm_hour = temp / SEC_IN_HOUR;						// Hour
        g_time.sl_tm_min = (temp % SEC_IN_HOUR) / SEC_IN_MIN;		// Minute
        g_time.sl_tm_sec = temp % SEC_IN_MIN;						// Seconds

        // Print current date/time to console
        char acDay[3];
        char acMonth[3];
        memcpy(acDay, g_acDaysOfWeek2013[(g_sAppData.ulElapsedSec / SEC_IN_DAY) % 7], 3);
        memcpy(acMonth, g_acMonthOfYear[g_time.sl_tm_mon], 3);
        long len = strlen(acDay); // Temporary solution to compiler bug causing compiler to resolve incorrect length for character arrays
        len = strlen(acMonth); // Temporary solution to compiler bug causing compiler to resolve incorrect length for character arrays
        UART_PRINT("%s %s %d %d:%d:%d UTC %d\n\r",
        			acDay,
        			acMonth,
        			g_time.sl_tm_day,
        			g_time.sl_tm_hour,
        			g_time.sl_tm_min,
        			g_time.sl_tm_sec,
        			g_time.sl_tm_year);
    }
    return 0;
}

// Update system time according to NTP server. Return 0 on success, negative on error
long updateSystemTime(void *pvParameters)
{
    long lRetVal = -1;
    long idx = 0;

	// Create UDP socket
	lRetVal = sl_Socket(SL_AF_INET, SL_SOCK_DGRAM, SL_IPPROTO_UDP);
	ASSERT_ON_ERROR(lRetVal);
	g_sAppData.iSockID = lRetVal;

    for(idx = 0; idx < NUM_NTP_SERVERS; idx++)
    {
    	UART_PRINT("[TIME] Fetching time from `%s` ...\n\r", g_acSNTPserver[idx]);

		// Get server IP
		lRetVal = sl_NetAppDnsGetHostByName((signed char *)g_acSNTPserver[idx],
											strlen(g_acSNTPserver),
											&g_sAppData.ulDestinationIP,
											SL_AF_INET);
		if(lRetVal >= 0)
		{

			struct SlTimeval_t timeVal;
			timeVal.tv_sec =  SERVER_RESPONSE_TIMEOUT;    // Seconds
			timeVal.tv_usec = 0;     // Microseconds. 10000 microseconds resolution
			lRetVal = sl_SetSockOpt(g_sAppData.iSockID,SL_SOL_SOCKET,SL_SO_RCVTIMEO,\
							(unsigned char*)&timeVal, sizeof(timeVal));
			if(lRetVal < 0)
			{
				ERR_PRINT(lRetVal);
			}
			else
			{
				// Get the NTP time and display the time
				lRetVal = GetSntpTime(GMT_DIFF_TIME_HRS, GMT_DIFF_TIME_MINS);
				if(lRetVal >= 0)
				{
					// Configure system time
					lRetVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
										  SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
										  sizeof(g_time),(unsigned char *)(&g_time));
					if(lRetVal < 0)
					{
						ERR_PRINT(lRetVal);
					}
					else
					{
						// Close the socket and return success
						sl_Close(g_sAppData.iSockID);
						return 0;
					}
				}
			}
		}
    }


	// Close the socket
	sl_Close(g_sAppData.iSockID);

    // Return error value
    return lRetVal;
}

//*****************************************************************************
//
//! itoa
//!
//!    @brief  Convert integer to ASCII in decimal base
//!
//!     @param  cNum is input integer number to convert
//!     @param  cString is output string
//!
//!     @return number of ASCII parameters
//!
//!
//
//*****************************************************************************
unsigned short itoa(short cNum, char *cString)
{
    char* ptr;
    short uTemp = cNum;
    unsigned short length;

    // value 0 is a special case
    if (cNum == 0)
    {
        length = 1;
        *cString = '0';

        return length;
    }

    // Find out the length of the number, in decimal base
    length = 0;
    while (uTemp > 0)
    {
        uTemp /= 10;
        length++;
    }

    // Do the actual formatting, right to left
    uTemp = cNum;
    ptr = cString + length;
    while (uTemp > 0)
    {
        --ptr;
        *ptr = g_acDigits[uTemp % 10];
        uTemp /= 10;
    }

    return length;
}
