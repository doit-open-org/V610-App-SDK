/*
 * Copyright (c) 2026 HiSilicon (Shanghai) Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @defgroup osal_rtc osal_rtc
 */
#ifndef OSAL_RTC_H
#define OSAL_RTC_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef struct {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
} osal_rtc_time;

/**
 * @ingroup osal_rtc
 * @brief Converts timestamp to rtc_time.
 *
 * @par Description:
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 *
 * @param time [in] The timestamp to be convert.
 * @param tm [out] Obtained rtc_time.
 *
 * @par Support System:
 * linux liteos.
 */
void osal_rtc_time_to_tm(unsigned long time, osal_rtc_time *tm);

/**
 * @ingroup osal_rtc
 * @brief Converts rtc_time to timestamp.
 *
 * @par Description:
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 *
 * @param tm [in] The rtc_time to be convert.
 * @param time [out] Obtained timestamp.
 *
 * @par Support System:
 * linux liteos.
 */
void osal_rtc_tm_to_time(const osal_rtc_time *tm, unsigned long *time);

/**
 * @ingroup osal_rtc
 * @brief Does the rtc_time represent a valid date/time?
 *
 * @par Description:
 * Does the rtc_time represent a valid date/time.
 *
 * @return OSAL_SUCCESS/OSAL_FAILURE
 *
 * @par Support System:
 * linux liteos.
 */
int osal_rtc_valid_tm(const osal_rtc_time *tm);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* OSAL_RTC_H */
