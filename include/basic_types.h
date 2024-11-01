/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef __BASIC_TYPES_H__
#define __BASIC_TYPES_H__

#include <stdio.h>
//#define ENABLE_DEBUG 1
#undef  LOG  // CID 282851 : Parse warning (PW.BAD_MACRO_REDEF)
#define LOG(level, text, ...) do {\
    printf("%s[%d] - %s: " text, __FUNCTION__, __LINE__, level, ##__VA_ARGS__);}while(0);

#define ERROR(text, ...) do {\
    printf("%s[%d] - %s: " text, __FUNCTION__, __LINE__, "ERROR", ##__VA_ARGS__);}while(0);
#define WARN(text, ...) do {\
    printf("%s[%d] - %s: " text, __FUNCTION__, __LINE__, "WARN", ##__VA_ARGS__);}while(0);
#define INFO(text, ...) do {\
    printf("%s[%d] - %s: " text, __FUNCTION__, __LINE__, "INFO", ##__VA_ARGS__);}while(0);

#ifdef ENABLE_DEBUG
#define DEBUG(text, ...) do {\
    printf("%s[%d] - %s: " text, __FUNCTION__, __LINE__, "DEBUG", ##__VA_ARGS__);}while(0);
#else
#define DEBUG(text, ...)
#endif


#define REPORT_IF_UNEQUAL(lhs, rhs) do { \
    if((lhs) != (rhs)) ERROR("Unexpected error!\n");}while(0);


typedef enum
{
	AUDIO_SETTINGS_CHANGE_EVENT = 0
}audio_capture_events_t;

#endif // __BASIC_TYPES_H__
