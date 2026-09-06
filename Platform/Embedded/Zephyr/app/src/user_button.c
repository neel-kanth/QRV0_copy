/**
 * @file      user_button.c
 *
 * @brief     Manage application launch via the user button.
 *
 * @author    Qorvo Paris Applications
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/shell/shell_uart.h>
#include <twr_app.h>

#define USER_BUTTON_DELAY_MS (1000)
#define DEBOUNCE_INTERVAL_MS (150)
#define UCI_BRIDGE_CMD "uci-bridge"
#define TWR_CMD "cherry-twr-app -C -n 0"

LOG_MODULE_REGISTER(button, CONFIG_APP_LOG_LEVEL);

struct user_button_data {
	struct gpio_callback cb_data;
	struct k_timer timer;
	struct k_work work;
	struct k_thread app_start_thread;
	int64_t last_push_time_ms;
	int push_count;
};

static struct user_button_data g_user_button_data;
static const struct gpio_dt_spec user_led =
    GPIO_DT_SPEC_GET(DT_ALIAS(user_led), gpios);
static const struct gpio_dt_spec user_button =
    GPIO_DT_SPEC_GET(DT_ALIAS(user_button), gpios);

static K_THREAD_STACK_DEFINE(app_start_stack, 1024);

static void user_button_pushed(const struct device *dev,
			       struct gpio_callback *cb, uint32_t pins)
{
	int64_t push_time_ms = k_uptime_get();

	/* Debouncing the button. */
	if (g_user_button_data.push_count &&
	    ((push_time_ms - g_user_button_data.last_push_time_ms) <
	     DEBOUNCE_INTERVAL_MS)) {
		return;
	}
	if (g_user_button_data.push_count == 0) {
		/* Start the timer at the first push. */
		k_timer_start(&g_user_button_data.timer,
			      K_MSEC(USER_BUTTON_DELAY_MS), K_NO_WAIT);
	}
	g_user_button_data.push_count++;
	g_user_button_data.last_push_time_ms = push_time_ms;
}

static void led_blink(int count)
{
	int i;

	for (i = 0; i < count; i++) {
		gpio_pin_set_dt(&user_led, 1);
		k_msleep(100);
		gpio_pin_set_dt(&user_led, 0);
		k_msleep(100);
	}
}
static void start_app_thread(void *arg0, void *arg1, void *arg2)
{
	shell_execute_cmd(shell_backend_uart_get_ptr(), arg0);
}

static void start_app(char *cmd)
{
	/* Since the shell command may use the sysworkq, the command is launched
	 * in a new thread to avoid a dead lock in the sysworkq. */
	k_thread_create(&g_user_button_data.app_start_thread, app_start_stack,
			K_THREAD_STACK_SIZEOF(app_start_stack),
			start_app_thread, cmd, NULL, NULL, 0, 0, K_NO_WAIT);
}

static void user_button_timer_work(struct k_work *work)
{
	switch (g_user_button_data.push_count) {
	case 1:
		LOG_INF("Launch uci bridge.");
		led_blink(g_user_button_data.push_count);
		start_app(UCI_BRIDGE_CMD);
		break;
	case 2:
		LOG_INF("Launch twr app.");
		led_blink(g_user_button_data.push_count);
		start_app(TWR_CMD);
		break;
	default:
		LOG_WRN("Bad button push count: %d, no action done.",
			g_user_button_data.push_count);
		/* Re-init the button app trigger. */
		g_user_button_data.push_count = 0;
		g_user_button_data.last_push_time_ms = 0;
		gpio_pin_interrupt_configure_dt(&user_button,
						GPIO_INT_EDGE_TO_ACTIVE);
		break;
	}
}

static void user_button_timer_expire(struct k_timer *timer)
{
	gpio_pin_interrupt_configure_dt(&user_button, GPIO_INT_DISABLE);
	k_work_submit(&g_user_button_data.work);
}

int user_button_init(void)
{
	int ret;

	k_timer_init(&g_user_button_data.timer, user_button_timer_expire, NULL);
	k_work_init(&g_user_button_data.work, user_button_timer_work);

	if (!gpio_is_ready_dt(&user_led)) {
		LOG_ERR("The user led gpio is not ready");
		return -EIO;
	}
	ret = gpio_pin_configure_dt(&user_led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Error in the user led gpio configuration, %d", ret);
		return ret;
	}
	if (!gpio_is_ready_dt(&user_button)) {
		LOG_ERR("The user button gpio is not ready");
		return -EIO;
	}
	ret = gpio_pin_configure_dt(&user_button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Error in the user button gpio configuration, %d", ret);
		return ret;
	}
	ret = gpio_pin_interrupt_configure_dt(&user_button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR(
		    "Error %d: failed to configure interrupt on %s pin %d\n",
		    ret, user_button.port->name, user_button.pin);
		return ret;
	}
	gpio_init_callback(&g_user_button_data.cb_data, user_button_pushed,
			   BIT(user_button.pin));
	gpio_add_callback(user_button.port, &g_user_button_data.cb_data);
	return 0;
}
