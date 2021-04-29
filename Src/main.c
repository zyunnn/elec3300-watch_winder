/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// #include "lcd.h"
 #include "stdbool.h"
 #include "lcdtp.h"
 #include "xpt2046.h"
 #include <stdio.h>
 #include <string.h>
 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
 #define DEFAULT_FONTSIZE 	16
 #define BASE_ADDR 					0x08007C00
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */
// TODO: define Task object
int windDir = 0; // 0=clockwise, 1=anticlockwise, 2=bi-directional
char *windDirName[3] = {"clockwise", "anti-clockwise", "bi-directional"};
int targetNumRotation, curNumRotation;
float targetHour = 24;
float curHour;

bool interruptFlag = false;
unsigned int interruptTimer = refractoryPeriod;
strType_XPT2046_TouchPara touchPara = {0.085958, -0.001073, -4.979353, -0.001750, 0.065168, -13.318824};

char *modelName[] = {"AP Royal Oak 2012", "CHOPARD LUC 1937", "HUBLOT 1915", "PIAGET Altiplano", "OMEGA Seamaster"};
int modelTurn[5] = {800, 800, 650, 1340, 650};
int modelWindDir[5] = {0,0,2,1,2};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FSMC_Init(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

	bool checkOngoing(int* prevRotation, float* prevHour, int* prevTargetRotation, float* prevTargetHour) {
		/*
		Check EEPROM for ongoing task
		*/
		// TODO
		return true;
	}

	bool saveData(char dataName[], double value) {
		uint64_t targetAddr = BASE_ADDR;
		if (!strcmp(dataName, "targetNumRotation")) {
			targetNumRotation = value;
			return true;
		} else if (!strcmp(dataName, "curNumRotation")) {
			curNumRotation = value;
			return true;
		} else if (!strcmp(dataName, "targetHour")) {
			targetHour = value;
			return true;
		} else if (!strcmp(dataName, "curHour")) {
			curHour = value;
			return true;
		} else if (!strcmp(dataName, "windDir")) {
			windDir = value;
			return true;
		}
		return false;
	}
	
	double loadData(char dataName[]) {
		if (!strcmp(dataName, "targetNumRotation")) {
			return targetNumRotation;
		} else if (!strcmp(dataName, "curNumRotation")) {
			return curNumRotation;
		} else if (!strcmp(dataName, "targetHour")) {
			return targetHour;
		} else if (!strcmp(dataName, "curHour")) {
			return curHour;
		} else if (!strcmp(dataName, "windDir")) {
			return windDir;
		}
		return -1;
	}
	
	void incrementInterruptTimer (int step) {
		interruptTimer = interruptTimer == refractoryPeriod? refractoryPeriod: interruptTimer + step;
	}
	
	bool isTouched(void) {
		bool touched = (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4)==GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
		return touched;
	}
	
	int selectInputType(void) {
		LCD_DrawString(50,50,"Configure run from", DEFAULT_FONTSIZE);
		LCD_DrawRectButton(50,150,150,40,"Model List", BLACK);
		LCD_DrawRectButton(50,200,150,40,"Manual Input", BLACK);
		HAL_Delay(300);
		
		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// option1: predefined model
				if ((int)coords.y >= 50 && (int)coords.y <= 200 && (int)coords.x >= 130 && (int)coords.x <= 170) {		// 320-150, 320-190
					return 0;
				}
				// option2: manual input
				else if ((int)coords.y >= 50 && (int)coords.y <= 200 && (int)coords.x >= 80 && (int)coords.x <= 120) {	// 320-200, 320-240
					return 1;
			}
				// invalid input
				else {
//					sprintf(buffer, "Please select valid input x: %d, y: %d", (int)coords.x, (int)coords.y);
					LCD_Clear(50,50,200,50,BACKGROUND);
					LCD_DrawString(40,50,"Please select valid input",8);
					HAL_Delay(500);
					LCD_Clear(0,50,240,50,BACKGROUND);
					LCD_DrawString(50,50,"Select input type", DEFAULT_FONTSIZE);
				}
		}
	}
}
	
	void selectModel(void) {
		/*
		Select watch model with predefined winding details
		*/
		char buffer[50];
		sprintf(buffer, "%d", targetNumRotation);	
		LCD_DrawString(45,50,"Select watch model",DEFAULT_FONTSIZE);
		int startP = 100, height = 40;
		// display 5 predefined models
		for(int i = 0; i < 5; i++) {
			LCD_DrawRectButton(30,startP+(i*height),170,height,modelName[i], BLACK);
		}		
		strType_XPT2046_Coordinate coords;
		while(true) {
			if(isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				if(coords.y >= 30 && coords.y <= 200) {
					int modelNo = (int)((320-coords.x-startP)/40);	
					LCD_Clear(10,50,200,270,BACKGROUND);
					LCD_DrawString(60,50,"Model selected:",DEFAULT_FONTSIZE);
					LCD_DrawString(50,100,modelName[modelNo],DEFAULT_FONTSIZE);
					saveData("targetNumRotation", modelTurn[modelNo]);
					saveData("windDir", modelWindDir[modelNo]);
					HAL_Delay(500);
					return;
				}
			}
		}
	}
	
	void inputTurn(void) {
		/*
		Manually input number of rotation turns
		*/
		char buffer[50];
		sprintf(buffer, "%d", (int)loadData("targetNumRotation"));
		
		LCD_DrawString(50,50,"Input rotation number",DEFAULT_FONTSIZE);
		LCD_DrawRectButton(50,150,150,40,"- 100 rotation", BLACK);
		LCD_DrawRectButton(50,200,150,40,"+ 100 rotation", BLACK);
		LCD_DrawString(170,300,"Next >>",DEFAULT_FONTSIZE);
		LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 	
		
		strType_XPT2046_Coordinate coords;
		while (true) {	
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// decrement by 100 rotation
				if (coords.y >= 50 && coords.y <= 200 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					saveData("targetNumRotation", loadData("targetNumRotation") - 100);
					sprintf(buffer, "%d", (int)loadData("targetNumRotation"));
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
					HAL_Delay(100);
				}
				// increment by 100 rotation
				else if (coords.y >= 50 && coords.y <= 200 && coords.x >= 80 && coords.x <= 120){		// 320-200, 320-240
					saveData("targetNumRotation", loadData("targetNumRotation") + 100);
					sprintf(buffer, "%d", (int)loadData("targetNumRotation"));
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
					HAL_Delay(100);
				}
				// finish input, move to next input option
				else if (coords.y >= 180 && coords.y <= 250 && coords.x >= 0 && coords.x <= 50){			// 320-180, 320-320
					return;
				}	
				// invalid input
				else {
					LCD_DrawString(20,100,"Please select valid input.",DEFAULT_FONTSIZE);
					HAL_Delay(1000);
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE);
					HAL_Delay(100);
				}
			}
		}
	}
	
	void inputHour(void) {
		/*
		Manually input winding hour
		*/
		char buffer[50];
		sprintf(buffer, "%0.1f hour", (double)loadData("targetHour"));
		
		LCD_DrawString(50,50,"Input winding hour",DEFAULT_FONTSIZE);
		LCD_DrawRectButton(50,150,100,40,"- 1/2 hour", BLACK);
		LCD_DrawRectButton(50,200,100,40,"+ 1/2 hour", BLACK);
		LCD_DrawString(170,300,"Next >>",DEFAULT_FONTSIZE);
		LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
		
		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// decrement by 1/2 hour
				if (coords.y >= 50 && coords.y <= 150 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					saveData("targetHour", loadData("targetHour") - 0.5);
					sprintf(buffer, "%0.1f	hour", (double)loadData("targetHour"));
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
					HAL_Delay(100);
				}
				// increment by 1/2 hour
				else if (coords.y >= 50 && coords.y <= 150 && coords.x >= 80 && coords.x <= 120){		// 320-200, 320-240
					saveData("targetHour", loadData("targetHour") + 0.5);
					sprintf(buffer, "%0.1f	hour", (double)loadData("targetHour"));
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
					HAL_Delay(100);
				}
				// finish input, move to next operation
				else if (coords.y >= 180 && coords.y <= 250 && coords.x >= 0 && coords.x <= 50){			// 320-180, 320-320
					return;
				}	
				// invalid input
				else {
					LCD_DrawString(20,100,"Please select valid input.",DEFAULT_FONTSIZE);
					HAL_Delay(1000);
					LCD_Clear (10, 100, 240, 50, BACKGROUND);
					LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE);
					HAL_Delay(100);
				}
			}
		}
	}

	bool startNewTask(void) {
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		LCD_DrawString (20, 50, "Starting new task...", DEFAULT_FONTSIZE);
		HAL_Delay(500);
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		int inputType = selectInputType();
		switch(inputType) {
			case 0:			// predefined input
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				selectModel();
				break;
			case 1: 		
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				inputTurn();
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				inputHour();
				break;
		}
		return true;
	}
	
	
	void rotateFullCycle(int mode) {
		int t = 2;
		char buffer[50];
		for (int i = 0; i < 512; i++){
			incrementInterruptTimer(1);
			switch (mode) {
				case 0:
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);
					break;
				case 1:
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_SET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
					HAL_Delay(t);

					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
					HAL_Delay(t);
			}
		}
	}
	

	
	void startRotate(void) {
		/*
		Rotate 
		*/
		LCD_Clear(50,50,200,200,BACKGROUND);
		LCD_DrawString(80,50,"Rotating...", DEFAULT_FONTSIZE);
		
		int dir = (int)loadData("windDir") == 2? 0: (int)loadData("windDir");
		int tnr = (int)loadData("targetNumRotation");
		//tnr = 20;  	 // check progressBar
		
		progressBar pb;
		pb.range = tnr;
		pb.left = 18;
		pb.top = 150;
		pb.width = 204;
		pb.height = 20;
		LCD_DrawRectButton(pb.left,pb.top,pb.width,pb.height,"",BLACK);

		for (int i = 0; i < tnr; i++) {
			LCD_Clear(50, 100, 200, 20, BACKGROUND);
			while(interruptFlag) {
				LCD_Clear(50, 100, 200, 20, BACKGROUND);
				incrementInterruptTimer(1);
			}
			rotateFullCycle(dir);
			LCD_UpdatePb(&pb, i+1,BLUE);
			if ((int)loadData("windDir") == 2) dir = !dir;
			HAL_Delay(1000);
			if (!saveData("curNumRotation", i)) break;
		}

	}
	
	void startWinding(void) {
		// TODO: start rotation, update progress bar
		char buffer1[50], buffer2[50], buffer3[50];
		sprintf(buffer1, "Number of turns: %d", (int)loadData("targetNumRotation"));
		sprintf(buffer2, "Duration: %0.1f hour", (double)loadData("targetHour"));
		sprintf(buffer3, "Direction: %s", windDirName[(int)loadData("windDir")]);
		
		// BONUS TODO: Display expected end time of winding process?
		LCD_DrawString(10, 50,buffer1,DEFAULT_FONTSIZE);
		LCD_DrawString(10, 80, buffer2,DEFAULT_FONTSIZE);
		LCD_DrawString(10, 110, buffer3,DEFAULT_FONTSIZE);
		LCD_DrawRectButton(70,150,100,50,"Start",BLACK);
		LCD_DrawString(20,300,"<< Back",DEFAULT_FONTSIZE);
		
		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				if (coords.y >= 50 && coords.y <= 200 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					LCD_Clear(0, 0, 240, 320, BACKGROUND);
					startRotate();
					break; 		// finish rotation
				}
			}
		}
	}

	
	void promptWatch(void) {
		LCD_DrawString(50,50,"Place watch to start winding.",DEFAULT_FONTSIZE);
	}
	
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	int prevRotation, prevTargetRotation;
	float prevHour, prevTargetHour;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
	macXPT2046_CS_DISABLE();
	LCD_INIT();
	
	// uncomment following block to try touchpad
//	LCD_Clear(50,80,140,70,RED);
//	LCD_DrawString(68,100,"TOUCHPAD DEMO");
//	HAL_Delay(2000);
//	while(!XPT2046_Touch_Calibrate());
//	LCD_GramScan(1);
//	LCD_Clear(0,0,240,320,GREY);
	//LCD_Clear(90,230,60,60,BLUE);
	
	// TODO: check EEPROM for ongoing task
	bool prevFlag = checkOngoing(&prevRotation, &prevHour, &prevTargetRotation, &prevTargetHour);
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	bool setupReady = false;
  while (1)
  {		
    if (prevFlag) {
			LCD_DrawString(30,50,"Continue previous task?",DEFAULT_FONTSIZE);
			LCD_DrawRectButton(30,150,70,50, "Yes", BLACK);
			LCD_DrawRectButton(130,150,70,50, "No", BLACK);
			while (!setupReady) {
				if (isTouched()) {
					strType_XPT2046_Coordinate coords;
					XPT2046_Get_TouchedPoint(&coords, &touchPara);
					// continue prev task
					if (coords.y >= 30 && coords.y <= 100 && coords.x <= 170 && coords.x >= 120) {		// 320-150,320-200
						saveData("curNumRotation", prevRotation);			
						saveData("curHour", prevHour);
						saveData("targetNumRotation", prevTargetRotation);
						saveData("targetHour", prevTargetHour);
						setupReady = true;
					}
					// start a new task
					else if (coords.y >= 130 && coords.y <= 200 && coords.x <= 170 && coords.x >= 120){		// 320-150, 320-200
						setupReady = startNewTask();
//						if (isTouched()) {
/*							LCD_Clear (0, 0, 240, 320, BACKGROUND);
							int inputType = selectInputType();
							switch(inputType) {
								case 0:			// predefined input
									LCD_Clear (0, 0, 240, 320, BACKGROUND);
									selectModel();
									setupReady = true;
									break;
								case 1: 		
									LCD_Clear (0, 0, 240, 320, BACKGROUND);
									inputTurn();
									LCD_Clear (0, 0, 240, 320, BACKGROUND);
									inputHour();
									setupReady = true;
									break;
							}
							*/
//						}
					}
				}
			}
		}
		// Setup new task
		else {
			setupReady = startNewTask();
		}
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		startWinding();
		prevFlag = false;
		setupReady = false;
		
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* EXTI4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
  /* EXTI9_5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2|GPIO_PIN_6|GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pins : PE2 PE0 PE1 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PE3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PE4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PE6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PD13 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* FSMC initialization function */
static void MX_FSMC_Init(void)
{

  /* USER CODE BEGIN FSMC_Init 0 */

  /* USER CODE END FSMC_Init 0 */

  FSMC_NORSRAM_TimingTypeDef Timing = {0};

  /* USER CODE BEGIN FSMC_Init 1 */

  /* USER CODE END FSMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FSMC_NORSRAM_DEVICE;
  hsram1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FSMC_NORSRAM_BANK1;
  hsram1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
  hsram1.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
  hsram1.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
  /* Timing */
  Timing.AddressSetupTime = 15;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 255;
  Timing.BusTurnAroundDuration = 15;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FSMC_ACCESS_MODE_A;
  /* ExtTiming */

  if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
  {
    Error_Handler( );
  }

  /** Disconnect NADV
  */

  __HAL_AFIO_FSMCNADV_DISCONNECTED();

  /* USER CODE BEGIN FSMC_Init 2 */

  /* USER CODE END FSMC_Init 2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
