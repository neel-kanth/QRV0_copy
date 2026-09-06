/*
 * Copyright (c) 2022 Qorvo, Inc
 *
 * All rights reserved.
 *
 * NOTICE: All information contained herein is, and remains the property
 * of Qorvo, Inc. and its suppliers, if any. The intellectual and technical
 * concepts herein are proprietary to Qorvo, Inc. and its suppliers, and
 * may be covered by patents, patent applications, and are protected by
 * trade secret and/or copyright law. Dissemination of this information
 * or reproduction of this material is strictly forbidden unless prior written
 * permission is obtained from Qorvo, Inc.
 *
 */

#include <FreeRTOS.h>
#include <qtime.h>
#include <stdint.h>
#include <task.h>

#define QTIME_CONT_TICK_TO_US(Ticks) ((TickType_t)(Ticks * 1000000 / configTICK_RATE_HZ))

int64_t qtime_get_uptime_us()
{
	return QTIME_CONT_TICK_TO_US(xTaskGetTickCount());
}

uint32_t qtime_get_sys_freq_hz()
{
	return configCPU_CLOCK_HZ;
}

#if !NRFX_SYSTICK_ENABLED

#define QTIME_CONT_TICK_TO_US(Ticks) ((TickType_t)(Ticks * 1000000 / configTICK_RATE_HZ))

#define QTIME_MS_TO_TICK(xTimeInMs) (xTimeInMs / portTICK_PERIOD_MS)
#define QTIME_US_TO_TICK(xTimeInUs) ((xTimeInUs / portTICK_PERIOD_MS) / 1000)

void qtime_msleep(int ms)
{
	const TickType_t ticks = QTIME_MS_TO_TICK(ms);
	/* Minimum delay is 1 tick. */
	vTaskDelay(ticks ? ticks : 1);
}

void qtime_usleep(int us)
{
	const TickType_t ticks = QTIME_US_TO_TICK(us);
	/* Minimum delay is 1 tick. */
	vTaskDelay(ticks ? ticks : 1);
}

#else
static bool systick_init = false;
#include <nrfx_systick.h>

void qtime_msleep(int ms)
{
	if (!systick_init) {
		/* Initialize ARM Systick to use in UWB stack delay counting.*/
		nrfx_systick_init();
		systick_init = true;
	}
	nrfx_systick_delay_ms(ms);
}

void qtime_usleep(int us)
{
	if (!systick_init) {
		/* Initialize ARM Systick to use in UWB stack delay counting.*/
		nrfx_systick_init();
		systick_init = true;
	}

	nrfx_systick_delay_us(us);
}
#endif
