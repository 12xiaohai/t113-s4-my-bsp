#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include <openamp/sunxi_helper/openamp.h>

#include "control_service.h"

#define CONTROL_SERVICE_NAME       "t113-control"
#define CONTROL_PERIOD_MS          1U
#define CONTROL_WATCHDOG_MS        500U
#define CONTROL_MIN_PERMILLE       (-1000)
#define CONTROL_MAX_PERMILLE       1000

struct control_state {
	volatile int enabled;
	volatile int target_permille;
	volatile int output_permille;
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
		 "STATUS enabled=%d target=%d output=%d cycles=%lu\n",
		 g_control.enabled, g_control.target_permille,
		 g_control.output_permille, (unsigned long)g_control.cycles);
	control_reply(reply);
}

static int control_rpmsg_callback(struct rpmsg_endpoint *ept, void *data,
				  size_t len, uint32_t src, void *priv)
{
	char command[64];
	size_t command_len;
	char *end;
	long value;

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
		g_control.target_permille = 0;
		control_reply_status();
	} else if (!strncmp(command, "SET ", 4)) {
		value = strtol(command + 4, &end, 10);
		if (*end != '\0' || value < CONTROL_MIN_PERMILLE ||
		    value > CONTROL_MAX_PERMILLE) {
			control_reply("ERR range=-1000..1000\n");
		} else {
			g_control.target_permille = (int)value;
			control_reply_status();
		}
	} else {
		control_reply("ERR commands=PING,GET,SET,ENABLE,DISABLE,STOP\n");
	}

	return 0;
}

static void control_rpmsg_unbind(struct rpmsg_endpoint *ept)
{
	(void)ept;
	g_control.enabled = 0;
	g_control.target_permille = 0;
	printf("control: Linux endpoint unbound, output forced safe\n");
}

static void control_loop_task(void *param)
{
	TickType_t wake_tick = xTaskGetTickCount();
	const TickType_t period_ticks = pdMS_TO_TICKS(CONTROL_PERIOD_MS);
	const TickType_t watchdog_ticks = pdMS_TO_TICKS(CONTROL_WATCHDOG_MS);

	(void)param;
	g_control.last_command_tick = wake_tick;

	for (;;) {
		TickType_t now = xTaskGetTickCount();

		if (g_control.enabled &&
		    (TickType_t)(now - g_control.last_command_tick) > watchdog_ticks) {
			g_control.enabled = 0;
			g_control.target_permille = 0;
		}

		/* PWM hardware ownership will be added after the RPMsg path is verified. */
		g_control.output_permille = g_control.enabled ?
			g_control.target_permille : 0;
		g_control.cycles++;

		vTaskDelayUntil(&wake_tick, period_ticks);
	}
}

int control_service_start_task(void)
{
	memset(&g_control, 0, sizeof(g_control));

	return xTaskCreate(control_loop_task, "control_loop", 1024, NULL,
			   configMAX_PRIORITIES - 3, NULL) == pdPASS ? 0 : -1;
}

int control_service_init_rpmsg(void)
{
	g_control_ept = openamp_ept_open(CONTROL_SERVICE_NAME, 0,
					 RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					 NULL, control_rpmsg_callback,
					 control_rpmsg_unbind);
	if (!g_control_ept)
		return -1;

	printf("control: RPMsg service '%s' ready, period=%ums watchdog=%ums\n",
	       CONTROL_SERVICE_NAME, CONTROL_PERIOD_MS, CONTROL_WATCHDOG_MS);
	return 0;
}

int control_service_get_output_permille(void)
{
	return g_control.output_permille;
}
