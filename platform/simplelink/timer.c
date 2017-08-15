/*
 * Copyright Swan Solutions Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <timer_interface.h>
#include <stdlib.h>
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"


uint32_t left_ms(Timer *timer)
{
	unsigned long secs = 0;
	unsigned short msecs = 0;
	unsigned long diff_ms = 0;

	// Get current time
    MAP_PRCMRTCGet(&secs, &msecs);

    diff_ms = (secs - timer->start_s) * 1000;
    diff_ms += ((long)msecs - (long)timer->start_ms);

    // Return 0 if expired
    if (diff_ms >= timer->length_ms) {
        timer->length_ms = 0;
        diff_ms = 0;
    }

    return (diff_ms);
}

bool has_timer_expired(Timer *timer)
{
    return (!left_ms(timer));
}

void countdown_ms(Timer *timer, uint32_t tms)
{
	// Set the timer parameters appropriately
    timer->length_ms = tms;
    MAP_PRCMRTCGet(&timer->start_s, &timer->start_ms);
}

void countdown_sec(Timer *timer, uint32_t tsec)
{
	// Convert to ms and start ms timer
    countdown_ms(timer, tsec * 1000);
}

void init_timer(Timer *timer)
{
	// Check that RTC has been configured
	if(!MAP_PRCMRTCInUseGet()) {
		// Set flag to indicate RTC has been configured
		MAP_PRCMRTCInUseSet();

		// Set RTC time to reference NOW as zero-time
		MAP_PRCMRTCSet(0, 0);
	}

	// Initialize the timer variable
    timer->start_s = 0;
    timer->start_ms = 0;
    timer->length_ms = 0;
}
