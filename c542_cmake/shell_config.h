#ifndef DEMO_SHELL_CONFIG_H
#define DEMO_SHELL_CONFIG_H

/* A static command table avoids linker-section changes in generated files. */
#define SHELL_USING_CMD_EXPORT 0
#define SHELL_TASK_WHILE 0
#define SHELL_MAX_NUMBER 1
#define SHELL_PARAMETER_MAX_NUMBER 4
#define SHELL_HISTORY_MAX_NUMBER 4
#define SHELL_DEFAULT_USER "c542"
#define SHELL_CLS_WHEN_LOGIN 0
#define SHELL_HELP_SHOW_PERMISSION 0
#define SHELL_GET_TICK() HAL_GetTick()

#include "stm32_hal.h"

#endif
