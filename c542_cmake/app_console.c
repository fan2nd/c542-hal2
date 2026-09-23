/**
  ******************************************************************************
  * file           : app_console.c
  * brief          : Letter Shell commands and interrupt-driven UART transport.
  ******************************************************************************
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "app_console.h"
#include "app_board.h"
#include "mx_led.h"
#include "mx_usart2.h"
#include "shell.h"
#include "stm32c5xx_ll_gpio.h"
#include "stm32c5xx_ll_usart.h"
#include <stdbool.h>
#include <string.h>

#define RX_QUEUE_SIZE 256U

static Shell shell;
static char shell_buffer[128 * (SHELL_HISTORY_MAX_NUMBER + 1)];
static hal_uart_handle_t *uart;
static uint8_t rx_byte;
static uint8_t rx_queue[RX_QUEUE_SIZE];
static volatile unsigned int rx_head;
static volatile unsigned int rx_tail;
static volatile bool rx_fault;
static bool discard_line;

static short shell_write(char *data, unsigned short size)
{
  unsigned short written = 0;
  /* Bound each write to about 1.4 ms at 115200 baud, including long help/banner output. */
  while (written < size)
  {
    unsigned short count = size - written > 16U ? 16U : size - written;
    app_board_poll();
    if (HAL_UART_Transmit(uart, data + written, count, 100U) != HAL_OK)
    {
      break;
    }
    written += count;
  }
  return (short)written;
}

void HAL_UART_RxCpltCallback(hal_uart_handle_t *huart, uint32_t size_byte,
                             hal_uart_rx_event_types_t rx_event)
{
  (void)rx_event;
  if (huart != uart)
  {
    return;
  }
  unsigned int next = (rx_head + 1U) % RX_QUEUE_SIZE;
  if ((size_byte != 1U) || (next == rx_tail))
  {
    rx_fault = true;
  }
  else if (!rx_fault)
  {
    rx_queue[rx_head] = rx_byte;
    __DMB();
    rx_head = next;
  }
  if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
  {
    rx_fault = true;
  }
}

void HAL_UART_ErrorCallback(hal_uart_handle_t *huart)
{
  if (huart == uart)
  {
    /* Recovery and diagnostics belong in the main loop, not the interrupt. */
    rx_fault = true;
  }
}

static int command_led(int argc, char **argv)
{
  if (argc != 2)
  {
    shellWriteString(&shell, "Usage: led on|off|toggle|status\r\n");
    return -1;
  }
  if (strcmp(argv[1], "on") == 0)
  {
    led_on(LED_0);
  }
  else if (strcmp(argv[1], "off") == 0)
  {
    led_off(LED_0);
  }
  else if (strcmp(argv[1], "toggle") == 0)
  {
    led_toggle(LED_0);
  }
  else if (strcmp(argv[1], "status") != 0)
  {
    shellWriteString(&shell, "Usage: led on|off|toggle|status\r\n");
    return -1;
  }
  shellWriteString(&shell, (LL_GPIO_ReadOutputPort(GPIOA) & LED_0_PIN)
                   ? "LED: on\r\n" : "LED: off\r\n");
  return 0;
}

static int command_sleep(int argc, char **argv)
{
  (void)argv;
  if (argc != 1)
  {
    shellWriteString(&shell, "Usage: sleep\r\n");
    return -1;
  }
  if (!app_board_can_sleep())
  {
    shellWriteString(&shell, "Release the button before sleep.\r\n");
    return -1;
  }

  /* Blocking TX waits for TC, so the final message is complete before disabling USART. */
  shellWriteString(&shell, "Stop1: LED off; press USER button to wake.\r\n");
  led_off(LED_0);
  NVIC_DisableIRQ(USART2_IRQn);
  NVIC_DisableIRQ(LPDMA1_CH0_IRQn);
  NVIC_DisableIRQ(LPDMA1_CH1_IRQn);
  if (HAL_UART_AbortReceive(uart) != HAL_OK)
  {
    NVIC_SystemReset();
  }
  LL_USART_Disable(USART2);
  rx_head = rx_tail = 0U;
  rx_fault = false;

  app_board_sleep();
  LL_USART_Enable(USART2);
  if ((HAL_UART_AbortReceive(uart) != HAL_OK) ||
      (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK))
  {
    NVIC_SystemReset();
  }
  NVIC_ClearPendingIRQ(USART2_IRQn);
  NVIC_ClearPendingIRQ(LPDMA1_CH0_IRQn);
  NVIC_ClearPendingIRQ(LPDMA1_CH1_IRQn);
  NVIC_EnableIRQ(LPDMA1_CH0_IRQn);
  NVIC_EnableIRQ(LPDMA1_CH1_IRQn);
  NVIC_EnableIRQ(USART2_IRQn);
  shellWriteString(&shell, "Wake: button; LED: on\r\n");
  return 0;
}

/* These built-ins are the upstream command-table interface (shell_cmd_list.c). */
extern void shellHelp(int argc, char *argv[]);
extern void shellUp(Shell *shell);
extern void shellDown(Shell *shell);
extern void shellLeft(Shell *shell);
extern void shellRight(Shell *shell);
extern void shellTab(Shell *shell);
extern void shellBackspace(Shell *shell);
extern void shellDelete(Shell *shell);
extern void shellEnter(Shell *shell);

const ShellCommand shellCommandList[] = {
  SHELL_USER_ITEM(0, c542, , local console),
  SHELL_CMD_ITEM(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 help, shellHelp, show command help),
  SHELL_CMD_ITEM(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 led, command_led, led on|off|toggle|status),
  SHELL_CMD_ITEM(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 sleep, command_sleep, LED off and Stop1; USER button wakes),
  SHELL_KEY_ITEM(0, 0x1B5B4100, shellUp, previous command),
  SHELL_KEY_ITEM(0, 0x1B5B4200, shellDown, next command),
  SHELL_KEY_ITEM(0, 0x1B5B4400, shellLeft, cursor left),
  SHELL_KEY_ITEM(0, 0x1B5B4300, shellRight, cursor right),
  SHELL_KEY_ITEM(0, 0x09000000, shellTab, completion),
  SHELL_KEY_ITEM(0, 0x08000000, shellBackspace, backspace),
  SHELL_KEY_ITEM(0, 0x7F000000, shellBackspace, backspace),
  SHELL_KEY_ITEM(0, 0x1B5B337E, shellDelete, delete),
  SHELL_KEY_ITEM(0, 0x0A000000, shellEnter, enter),
  SHELL_KEY_ITEM(0, 0x0D000000, shellEnter, enter),
};
const unsigned short shellCommandCount = sizeof(shellCommandList) / sizeof(shellCommandList[0]);

int app_console_init(void)
{
  uart = mx_usart2_uart_gethandle();
  shell.write = shell_write;
  shellInit(&shell, shell_buffer, sizeof(shell_buffer));
  if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
  {
    return -1;
  }

  return 0;
}

void app_console_poll(void)
{
  if (rx_fault)
  {
    NVIC_DisableIRQ(USART2_IRQn);
    if (HAL_UART_AbortReceive(uart) != HAL_OK)
    {
      NVIC_SystemReset();
    }
    rx_head = rx_tail = 0U;
    rx_fault = false;
    shell.parser.length = shell.parser.cursor = 0;
    shell.parser.keyValue = 0;
    discard_line = true;
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
    {
      NVIC_SystemReset();
    }
    NVIC_ClearPendingIRQ(USART2_IRQn);
    NVIC_EnableIRQ(USART2_IRQn);
    shellWriteString(&shell, "\r\nRX error/overflow: line discarded. Press Enter and retry.\r\n");
  }
  if (rx_tail != rx_head)
  {
    char data = (char)rx_queue[rx_tail];
    __DMB();
    rx_tail = (rx_tail + 1U) % RX_QUEUE_SIZE;
    if (discard_line)
    {
      if ((data != '\r') && (data != '\n'))
      {
        return;
      }
      discard_line = false;
    }
    shellHandler(&shell, data);
    /* Reject an oversized line as a whole; never execute a silently truncated command. */
    if (shell.parser.length >= shell.parser.bufferSize - 1)
    {
      shell.parser.length = shell.parser.cursor = 0;
      shell.parser.keyValue = 0;
      discard_line = true;
      shellWriteString(&shell, "\r\nLine too long: discarded. Press Enter and retry.\r\n");
    }
  }
}
