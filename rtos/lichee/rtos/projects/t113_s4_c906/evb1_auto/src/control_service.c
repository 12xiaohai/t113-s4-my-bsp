#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include <openamp/sunxi_helper/openamp.h>
#include <openamp/sunxi_helper/rpmsg_master.h>
#include <sunxi_hal_pwm.h>

#include "control_service.h"

#define CONTROL_SERVICE_NAME       "t113-control"
#define CONTROL_PERIOD_MS          1U
#define CONTROL_WATCHDOG_MS        500U
#define CONTROL_PWM_PERIOD_NS      200000U
#define CONTROL_PWM_COUNT          3U
#define CONTROL_MIN_PERMILLE       0
#define CONTROL_MAX_PERMILLE       1000

/* PWM4/5/6 are routed to PD5/PD6/PD7 on the LCD connector. */
static const int g_pwm_channels[CONTROL_PWM_COUNT] = { 4, 5, 6 };

struct control_state {
	volatile int enabled;
	volatile int target_permille[CONTROL_PWM_COUNT];
	volatile int output_permille[CONTROL_PWM_COUNT];
	volatile uint32_t cycles;
	volatile TickType_t last_command_tick;
};

static struct control_state g_control;
static struct rpmsg_endpoint *g_control_ept;

static void control_reply(const char *message)
{
	if (g_control_ept && message)
		/* The Tina OpenAMP API predates const-correct payload arguments. */
		openamp_rpmsg_send(g_control_ept, (void *)message,
				   (uint32_t)strlen(message));
}

static void control_reply_status(void)
{
	char reply[128];

	snprintf(reply, sizeof(reply),
		 "STATUS enabled=%d target=%d,%d,%d output=%d,%d,%d cycles=%lu\n",
		 g_control.enabled,
		 g_control.target_permille[0], g_control.target_permille[1],
		 g_control.target_permille[2], g_control.output_permille[0],
		 g_control.output_permille[1], g_control.output_permille[2],
		 (unsigned long)g_control.cycles);
	control_reply(reply);
}

static int control_rpmsg_callback(struct rpmsg_endpoint *ept, void *data,
				  size_t len, uint32_t src, void *priv)
{
	char command[64];
	size_t command_len;
	int channel;
	int value;
	int value1;
	int value2;
	int parsed;
	unsigned int i;

	(void)ept;
	(void)src;
	(void)priv;

	command_len = len < sizeof(command) - 1 ? len : sizeof(command) - 1;
	memcpy(command, data, command_len);
	command[command_len] = '\0';
	while (command_len &&
	       (command[command_len - 1] == '\n' || command[command_len - 1] == '\r'))
		command[--command_len] = '\0';

	g_control.last_command_tick = xTaskGetTickCount();

	if (!strcmp(command, "PING")) {
		control_reply("PONG c906\n");
	} else if (!strcmp(command, "GET")) {
		control_reply_status();
	} else if (!strcmp(command, "ENABLE")) {
		g_control.enabled = 1;
		control_reply_status();
	} else if (!strcmp(command, "DISABLE") || !strcmp(command, "STOP")) {
		g_control.enabled = 0;
		for (i = 0; i < CONTROL_PWM_COUNT; i++)
			g_control.target_permille[i] = 0;
		control_reply_status();
	} else if (!strncmp(command, "SETALL ", 7)) {
		parsed = sscanf(command + 7, "%d %d %d", &value, &value1, &value2);
		if (parsed != 3 || value < CONTROL_MIN_PERMILLE ||
		    value > CONTROL_MAX_PERMILLE || value1 < CONTROL_MIN_PERMILLE ||
		    value1 > CONTROL_MAX_PERMILLE || value2 < CONTROL_MIN_PERMILLE ||
		    value2 > CONTROL_MAX_PERMILLE) {
			control_reply("ERR SETALL duty1 duty2 duty3; range=0..1000\n");
		} else {
			g_control.target_permille[0] = value;
			g_control.target_permille[1] = value1;
			g_control.target_permille[2] = value2;
			control_reply_status();
		}
	} else if (!strncmp(command, "SET ", 4)) {
		parsed = sscanf(command + 4, "%d %d", &channel, &value);
		if (parsed == 1 && channel >= CONTROL_MIN_PERMILLE &&
		    channel <= CONTROL_MAX_PERMILLE) {
			/* Backward-compatible form: SET duty applies to all outputs. */
			for (i = 0; i < CONTROL_PWM_COUNT; i++)
				g_control.target_permille[i] = channel;
			control_reply_status();
		} else if (parsed != 2 || channel < 1 ||
			   channel > (int)CONTROL_PWM_COUNT ||
			   value < CONTROL_MIN_PERMILLE ||
			   value > CONTROL_MAX_PERMILLE) {
			control_reply("ERR SET channel duty; channel=1..3 duty=0..1000\n");
		} else {
			g_control.target_permille[channel - 1] = value;
			control_reply_status();
		}
	} else {
		control_reply("ERR commands=PING,GET,SET,SETALL,ENABLE,DISABLE,STOP\n");
	}

	return 0;
}

static int control_rpmsg_bind(struct rpmsg_ept_client *client)
{
	if (!client || !client->ept)
		return -1;

	g_control_ept = client->ept;
	printf("control: Linux endpoint /dev/rpmsg%lu bound\n",
	       (unsigned long)client->id);
	return 0;
}

static int control_rpmsg_unbind(struct rpmsg_ept_client *client)
{
	if (client && g_control_ept == client->ept)
		g_control_ept = NULL;
	g_control.enabled = 0;
	memset((void *)g_control.target_permille, 0,
	       sizeof(g_control.target_permille));
	printf("control: Linux endpoint unbound, output forced safe\n");
	return 0;
}

static void control_loop_task(void *param)
{
	TickType_t wake_tick = xTaskGetTickCount();
	const TickType_t period_ticks = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
	const TickType_t watchdog_ticks = pdMS_TO_TICKS(CONTROL_WATCHDOG_MS);
	struct pwm_config config;
	unsigned int i;
	int requested;

	(void)param;
	g_control.last_command_tick = wake_tick;

	for (;;) {
		TickType_t now = xTaskGetTickCount();

		if (g_control.enabled &&
		    (TickType_t)(now - g_control.last_command_tick) > watchdog_ticks) {
			g_control.enabled = 0;
			memset((void *)g_control.target_permille, 0,
			       sizeof(g_control.target_permille));
		}

		for (i = 0; i < CONTROL_PWM_COUNT; i++) {
			requested = g_control.enabled ? g_control.target_permille[i] : 0;
			if (g_control.output_permille[i] == requested)
				continue;

			config.period_ns = CONTROL_PWM_PERIOD_NS;
			config.duty_ns = (CONTROL_PWM_PERIOD_NS * requested) / 1000U;
			config.polarity = PWM_POLARITY_NORMAL;
			if (hal_pwm_control(g_pwm_channels[i], &config) == HAL_PWM_STATUS_OK)
				g_control.output_permille[i] = requested;
		}
		g_control.cycles++;

		vTaskDelayUntil(&wake_tick, period_ticks);
	}
}

int control_service_start_task(void)
{
	struct pwm_config config;
	unsigned int i;

	/* The vendor public PWM header carries its pin table as a static object. */
	(void)pwm_gpio;
	memset(&g_control, 0, sizeof(g_control));
	if (hal_pwm_init() != HAL_PWM_STATUS_OK)
		return -1;

	config.duty_ns = 0;
	config.period_ns = CONTROL_PWM_PERIOD_NS;
	config.polarity = PWM_POLARITY_NORMAL;
	for (i = 0; i < CONTROL_PWM_COUNT; i++) {
		if (hal_pwm_control(g_pwm_channels[i], &config) != HAL_PWM_STATUS_OK)
			return -1;
	}

	return xTaskCreate(control_loop_task, "control_loop", 1024, NULL,
			   configMAX_PRIORITIES - 3, NULL) == pdPASS ? 0 : -1;
}

int control_service_init_rpmsg(void)
{
	/*
	 * Linux creates the data endpoint through Allwinner's rpmsg_ctrl device.
	 * Registering a listener here lets the control daemon accept that request;
	 * a plain openamp_ept_open() endpoint is not visible to rpmsg_demo and the
	 * Linux driver reports "Remote don't listen this name".
	 */
	if (rpmsg_client_bind(CONTROL_SERVICE_NAME, control_rpmsg_callback,
			      control_rpmsg_bind, control_rpmsg_unbind,
			      1, NULL))
		return -1;

	printf("control: RPMsg service '%s' listening, period=%ums watchdog=%ums\n",
	       CONTROL_SERVICE_NAME, CONTROL_PERIOD_MS, CONTROL_WATCHDOG_MS);
	return 0;
}

int control_service_get_output_permille(void)
{
	return g_control.output_permille[0];
}
