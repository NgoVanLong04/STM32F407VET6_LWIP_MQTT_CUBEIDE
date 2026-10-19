#include "main.h"
#include "cmsis_os.h"

void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(500);
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
  }
  /* USER CODE END StartTask03 */
}
