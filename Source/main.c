#include "main.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

#define SEQ_LEN 8

uint8_t sequence_lvl1[SEQ_LEN] = {0, 0, 2, 1, 0, 2, 2, 1};
uint8_t sequence_lvl2[SEQ_LEN] = {2, 0, 1, 2, 1, 0, 1, 2};
uint8_t sequence_lvl3[SEQ_LEN] = {1, 2, 0, 1, 1, 2, 0, 2};

uint8_t *sequence = sequence_lvl1;
uint8_t input[SEQ_LEN];
uint8_t input_index = 0;
uint8_t game_state = 0;
uint8_t level = 1;
char uart_buf[128];

void SystemClock_Config(void);
void Error_Handler(void);
uint8_t read_color_button(void);

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

void LED_Off(void) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
}

void LED_ShowColor(uint8_t color) {
  LED_Off();
  switch(color) {
    case 0: HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); break; // Verde
    case 1: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET); break; // Roșu
    case 2: HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); break; // Albastru
    default: break;
  }
}

void play_sequence(void) {
  uint16_t delay_on = (level == 1) ? 1000 : (level == 2) ? 600 : 200;
  uint16_t delay_off = 500;
  for (int i = 0; i < SEQ_LEN; i++) {
    LED_ShowColor(sequence[i]);
    HAL_Delay(delay_on);
    LED_Off();
    HAL_Delay(delay_off);
  }
}

uint8_t read_color_button(void) {
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) return 0;
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) return 1;
  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) return 2;
  return 255;
}

uint8_t validate_sequence(void) {
  for (int i = 0; i < SEQ_LEN; i++) {
    if (sequence[i] != input[i]) return 0;
  }
  return 1;
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM2_Init();

  HAL_UART_Transmit(&huart1, (uint8_t *)"JOC SIMON SAYS\r\n", 16, HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart1, (uint8_t *)"Apasa USER pentru a incepe nivelul 1...\r\n", 39, HAL_MAX_DELAY);

  while (1)
  {
    switch (game_state)
    {
      case 0:  // Așteaptă start joc
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
          HAL_Delay(200);
          while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
          input_index = 0;
          LED_Off();
          game_state = 1;
        }
        break;

      case 1:  // Redă secvența
        input_index = 0; // reset la începutul secvenței pentru a evita loop-uri
        play_sequence();
        game_state = 2;
        break;

      case 2:  // Așteaptă input jucător
        if (input_index < SEQ_LEN) {
          uint8_t btn = read_color_button();
          if (btn != 255) {
            LED_ShowColor(btn);
            HAL_Delay(300);
            LED_Off();

            // debounce: așteaptă eliberarea butonului
            while (read_color_button() != 255) {
              HAL_Delay(10);
            }

            if (btn != sequence[input_index]) {
              HAL_UART_Transmit(&huart1, (uint8_t *)"Ai gresit. Cooldown 3 secunde...\r\n", 36, HAL_MAX_DELAY);
              for (int sec = 3; sec > 0; sec--) {
                snprintf(uart_buf, sizeof(uart_buf), "%d...\r\n", sec);
                HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
                HAL_Delay(1000);
              }
              HAL_UART_Transmit(&huart1, (uint8_t *)"Apasa USER pentru a revedea secventa.\r\n", 40, HAL_MAX_DELAY);
              input_index = 0;
              game_state = 0;
            } else {
              input[input_index++] = btn;
              if (input_index == SEQ_LEN) {
                game_state = 3;
              }
            }
          }
        }
        break;

      case 3:  // Felicitări și așteaptă next level
        snprintf(uart_buf, sizeof(uart_buf), "Felicitari! Treci la nivelul urmator apasand USER.\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), HAL_MAX_DELAY);
        game_state = 4;
        break;

      case 4:  // Așteaptă buton pentru next level
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
          HAL_Delay(200);
          while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
          input_index = 0;
          if (level == 1) {
            level = 2;
            sequence = sequence_lvl2;
            HAL_UART_Transmit(&huart1, (uint8_t *)"Nivelul 2 incepe...\r\n", 23, HAL_MAX_DELAY);
            game_state = 1;
          } else if (level == 2) {
            level = 3;
            sequence = sequence_lvl3;
            HAL_UART_Transmit(&huart1, (uint8_t *)"Nivelul 3 incepe...\r\n", 23, HAL_MAX_DELAY);
            game_state = 1;
          } else {
            HAL_UART_Transmit(&huart1, (uint8_t *)"Ai terminat toate nivelurile!\r\n", 34, HAL_MAX_DELAY);
            game_state = 5;
          }
        }
        break;

      case 5:  // Mesaj final, fără așteptare
        // Jocul s-a terminat, poți bloca aici sau adăuga alt comportament
        break;
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { Error_Handler(); }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
