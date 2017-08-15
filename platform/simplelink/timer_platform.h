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

#ifndef PLATFORM_SIMPLELINK_TIMER_PLATFORM_H_
#define PLATFORM_SIMPLELINK_TIMER_PLATFORM_H_

struct Timer {
    unsigned long start_s;
    unsigned short start_ms;
    unsigned long length_ms;
};

#endif
