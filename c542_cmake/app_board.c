#include "app_board.h"
#include "mx_button.h"
#include "mx_led.h"

#define BUTTON_DEBOUNCE_MS 20U

static volatile bool button_wake;
static bool button_sample;
static bool button_pressed;
static uint32_t button_changed_at;

static void button_interrupt(hal_exti_handle_t *hexti, hal_exti_trigger_t trigger)
{
  (void)hexti;
  (void)trigger;
  /* No tick-based debounce here: SysTick is suspended while waiting in Stop1. */
  button_wake = true;
}

int app_board_init(void)
{
  led_off(LED_0);
  /* C542 SRAM2 has two retention pages; preserve both stack and shell state in Stop1. */
  if ((HAL_PWR_LP_EnableMemoryRetention(HAL_PWR_MEMORY_RETENTION_SRAM1_STOP) != HAL_OK) ||
      (HAL_PWR_LP_EnableMemoryPageRetention(HAL_PWR_MEMORY_RETENTION_SRAM2_STOP, 1U, 2U) != HAL_OK) ||
      (HAL_EXTI_RegisterTriggerCallback(mx_gpio_default_exti13_gethandle(), button_interrupt) != HAL_OK))
  {
    return -1;
  }
  HAL_RCC_SetClockAfterWakeFromStop(HAL_RCC_STOP_WAKEUPCLOCK_HSIDIV3);
  return 0;
}

void app_board_poll(void)
{
  bool pressed = HAL_GPIO_ReadPin(BUTTON_0_GPIO_PORT, BUTTON_0_GPIO_PIN)
                 == BUTTON_0_GPIO_ACTIVE_STATE;
  uint32_t now = HAL_GetTick();
  if (pressed != button_sample)
  {
    button_sample = pressed;
    button_changed_at = now;
  }
  if ((pressed != button_pressed) && ((uint32_t)(now - button_changed_at) >= BUTTON_DEBOUNCE_MS))
  {
    button_pressed = pressed;
    if (pressed)
    {
      led_toggle(LED_0);
    }
  }
}

bool app_board_can_sleep(void)
{
  return !button_pressed && !button_sample &&
         HAL_GPIO_ReadPin(BUTTON_0_GPIO_PORT, BUTTON_0_GPIO_PIN) != BUTTON_0_GPIO_ACTIVE_STATE;
}

void app_board_sleep(void)
{
  /* Keep EXTI13 armed. Do not clear it after the final GPIO check or a press can be lost. */
  __disable_irq();
  button_wake = false;
  HAL_EXTI_ClearPending(mx_gpio_default_exti13_gethandle(), HAL_EXTI_TRIGGER_RISING_FALLING);
  NVIC_ClearPendingIRQ(EXTI13_IRQn);
  button_wake = HAL_GPIO_ReadPin(BUTTON_0_GPIO_PORT, BUTTON_0_GPIO_PIN) == BUTTON_0_GPIO_ACTIVE_STATE;
  __enable_irq();

  for (;;)
  {
    HAL_SuspendTick();
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    do
    {
      /* PRIMASK closes the check/WFI race; an enabled pending IRQ still releases WFI. */
      __disable_irq();
      if (!button_wake)
      {
        HAL_PWR_EnterStopMode(HAL_PWR_LOW_PWR_MODE_WFI, HAL_PWR_STOP1_MODE);
      }
      __enable_irq();
      __ISB();
    } while (!button_wake);

    /* Restore the tick at the wake clock first, so RCC timeouts remain bounded.
       Re-enable retained oscillator settings: mx_rcc_init is not safe if WFI did not sleep. */
    if ((HAL_UpdateCoreClock() != HAL_OK) ||
        (HAL_RCC_HSE_Enable(HAL_RCC_HSE_ON) != HAL_OK) ||
        (HAL_RCC_PSIS_Enable() != HAL_OK) ||
        (HAL_RCC_SetSYSCLKSource(HAL_RCC_SYSCLK_SRC_PSIS) != HAL_OK) ||
        (HAL_UpdateCoreClock() != HAL_OK))
    {
      NVIC_SystemReset();
    }

    /* A noisy EXTI edge may wake the core, but only a debounced press resumes the demo. */
    button_wake = false;
    HAL_Delay(BUTTON_DEBOUNCE_MS);
    if (HAL_GPIO_ReadPin(BUTTON_0_GPIO_PORT, BUTTON_0_GPIO_PIN) == BUTTON_0_GPIO_ACTIVE_STATE)
    {
      break;
    }
  }

  /* Consume this press once; the normal debouncer must see a release before another toggle. */
  button_sample = button_pressed = true;
  button_changed_at = HAL_GetTick();
  led_on(LED_0);
}
