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

#include "stdbool.h"
#include "lcdtp.h"
#include "xpt2046.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
	#define DEFAULT_FONTSIZE 		16
	#define	ADDR0								0x08017400
	#define	ADDR1								0x08017800
	#define	ADDR2								0x08016000
	#define	ADDR3								0x08016800
	#define ADDR4			 					0x08017C00
	#define NUM_PAGE						7
	#define DEFAULT_ROTATION		700
	#define MIN_ROTATION				0
	#define MAX_ROTATION				1000
	#define DEFAULT_HOUR				2.0
	#define MAX_HOUR						36.0
 
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
	SRAM_HandleTypeDef hsram1;

/* USER CODE BEGIN PV */
	char *windDirName[3] = {"clockwise", "anti-clockwise", "bi-directional"};
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

	uint32_t eraseAllData(void) {
		/*
		Erase all data of previous task in flash memory
		*/
		static FLASH_EraseInitTypeDef EraseInitStruct;
		uint64_t startAddr = ADDR2;
		uint32_t pageError = 0;
		
		HAL_FLASH_Unlock();

		EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
		EraseInitStruct.PageAddress = startAddr;
		EraseInitStruct.NbPages = NUM_PAGE;

		if (HAL_FLASHEx_Erase(&EraseInitStruct, &pageError) != HAL_OK) {
			return HAL_FLASH_GetError();		// error during page erase
		}
		
		HAL_FLASH_Lock();
		return 0;
	}
	

	uint16_t loadData(char dataName[]) {
		/*
		Read intial Task data from flash memory
		*/
		uint64_t* dataAddr;
		if (!strcmp(dataName, "targetNumRotation")) {
			dataAddr = (uint64_t*) ADDR0;
		} else if (!strcmp(dataName, "curNumRotation")) {
			dataAddr = (uint64_t*) ADDR1;
		} else if (!strcmp(dataName, "targetHour")) {
			dataAddr = (uint64_t*) ADDR2;
		} else if (!strcmp(dataName, "curHour")) {
			dataAddr = (uint64_t*) ADDR3;
		} else if (!strcmp(dataName, "windDir")) {
			dataAddr = (uint64_t*) ADDR4;
		}
		return *dataAddr;
	}
	
	
	uint32_t saveData(char dataName[], double value) {
		/*
		Write new Task data to flash memory
		
		Notes: previous Task data (if exists) is not erased until new Task data is input
		*/
		uint64_t targetAddr;
		
		if (!strcmp(dataName, "targetNumRotation")) {
			targetAddr = ADDR0;
		} else if (!strcmp(dataName, "curNumRotation")) {
			targetAddr = ADDR1;
		} else if (!strcmp(dataName, "targetHour")) {
			targetAddr = ADDR2;
		} else if (!strcmp(dataName, "curHour")) {
			targetAddr = ADDR3;
		} else if (!strcmp(dataName, "windDir")) {
			targetAddr = ADDR4;
		}
		
		HAL_FLASH_Unlock();
		
		if (!strcmp(dataName, "curNumRotation")) {
			int tnr = (int)loadData("targetNumRotation");
			int thr = (int)loadData("targetHour");
			float chr = (float)loadData("curHour");
			eraseAllData();
			saveData("targetNumRotation",tnr);
			saveData("targetHour",thr);
			saveData("curHour",chr);
		}
	
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, targetAddr, (uint16_t)value) != HAL_OK) 
				return HAL_FLASH_GetError();

		HAL_FLASH_Lock();
		return 0;
	}


	bool checkOngoing(windingTask* task) {
		/*
		Check and load previous Task data in flash memory
		
		Notes: do sanity check here determine whether flash memory data is valid
		*/
		task->targetNumRotation = (int)loadData("targetNumRotation");
		task->curNumRotation = (int)loadData("curNumRotation");
		task->targetHour = (float)loadData("targetHour")/10;
		task->curHour = (float)loadData("curHour");
		task->windDir = (int)loadData("windDir");

		// Check if all data are within valid range
		if (task->targetNumRotation > task->curNumRotation && task->targetHour > task->curHour && \
			  task->targetNumRotation <= MAX_ROTATION && task->targetHour <= MAX_HOUR && 
		    task->curNumRotation >= MIN_ROTATION && task->curHour >= 0 && \
				task->windDir >= 0 && task->windDir <= 2)
		return true;
		return false;
	}
	
	
	void incrementInterruptTimer (int step) {
		/* 
		Helper function for interrupt
		*/
		interruptTimer = interruptTimer == refractoryPeriod? refractoryPeriod: interruptTimer + step;
	}
	
	bool isTouched(void) {
		/*
		Helper function to check touchscreen status
		*/
		bool touched = (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4)==GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
		return touched;
	}
	
	int selectInputType(void) {
		/*
		Helper function to choose predefined model/manual input
		*/
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
		}
	}
}
	
	void selectModel(windingTask* task) {
		/*
		Select watch model with predefined winding details
		*/
		int startP = 100, height = 40;
		
		// display 5 predefined models
		LCD_DrawString(50,50,"Select watch model",DEFAULT_FONTSIZE);
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
					task->targetNumRotation = modelTurn[modelNo];
					task->windDir = modelWindDir[modelNo];
					task->targetHour = 7*modelTurn[modelNo]/3600.0;
					
					// save new Task data to memory
					eraseAllData();
					saveData("targetNumRotation",modelTurn[modelNo]);
					saveData("targetHour",(int)(task->targetHour*10));
					saveData("curNumRotation",0);
					saveData("curHour", 0.0); 
					saveData("windDir",modelWindDir[modelNo]);
					LCD_DrawString(50,50,"Model selected",DEFAULT_FONTSIZE);
					LCD_DrawString(30,100,modelName[modelNo],DEFAULT_FONTSIZE);
					HAL_Delay(1000);
					
					// configure new Task locally
					int tnr = (int)loadData("targetNumRotation");
					int cnr = (int)loadData("curNumRotation");
					int dir = (int)loadData("windDir");
					float thr = (float)loadData("targetHour")/10;
					float chr = (float)loadData("curHour");
					
					// display winding details of predefined model
					char buffer[50];
					LCD_Clear(0,0,240,320,BACKGROUND);
					sprintf(buffer, "rotation: %d, %d", tnr, cnr);
					LCD_DrawString(10,50,buffer,DEFAULT_FONTSIZE);
					sprintf(buffer, "hour: %0.1f, %0.1f", thr, chr);
					LCD_DrawString(10,70,buffer,DEFAULT_FONTSIZE);
					sprintf(buffer, "direction: %d", dir);
					LCD_DrawString(10,90,buffer,DEFAULT_FONTSIZE);
					HAL_Delay(1000);
					return;
				}
			}
		}
	}
	
	void inputTurn(windingTask* task) {
		/*
		Manually input number of rotation turns
		*/
		int tmpRotation = DEFAULT_ROTATION;
		char buffer[50];
		
		sprintf(buffer, "%d", tmpRotation);
		LCD_DrawString(50,50,"Input rotation number",DEFAULT_FONTSIZE);
		LCD_DrawRectButton(50,150,150,40,"- 100 rotation", BLACK);
		LCD_DrawRectButton(50,200,150,40,"+ 100 rotation", BLACK);
		LCD_DrawString(170,300,"Next >>",DEFAULT_FONTSIZE);
		LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 		// default rotation
		
		strType_XPT2046_Coordinate coords;
		while (true) {	
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// decrement by 100 rotation
				if (tmpRotation > MIN_ROTATION && coords.y >= 50 && coords.y <= 200 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					tmpRotation -= 100;
					sprintf(buffer, "%d", tmpRotation);
				}
				// increment by 100 rotation
				else if (tmpRotation < MAX_ROTATION && coords.y >= 50 && coords.y <= 200 && coords.x >= 80 && coords.x <= 120){		// 320-200, 320-240
					tmpRotation += 100;
					sprintf(buffer, "%d", tmpRotation);
				}
				// finish input, move to next input option
				else if (coords.y >= 180 && coords.y <= 250 && coords.x > 0 && coords.x <= 50){			// 320-180, 320-320
					task->targetNumRotation = tmpRotation;
					saveData("targetNumRotation", tmpRotation);
					return;
				}	
				LCD_Clear (10, 100, 240, 50, BACKGROUND);
				LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE);
				HAL_Delay(100);
			}
		}
	}
	
	void inputHour(windingTask* task) {
		/*
		Manually input winding hour
		*/
		float tmpHour = DEFAULT_HOUR;
		char buffer[50];
		
		sprintf(buffer, "%0.1f hour", tmpHour);
		LCD_Clear(0,0,240,320,BACKGROUND);
		LCD_DrawString(50,50,"Input winding hour", DEFAULT_FONTSIZE);
		LCD_DrawRectButton(50,150,100,40,"- 1/2 hour", BLACK);
		LCD_DrawRectButton(50,200,100,40,"+ 1/2 hour", BLACK);
		LCD_DrawString(170,300,"Next >>", DEFAULT_FONTSIZE);
		LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE);
		double MIN_HOUR = (double)loadData("targetNumRotation")*7/3600.0;
		
		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// decrement by 1/2 hour
				if (tmpHour - 0.5 >= MIN_HOUR && coords.y >= 50 && coords.y <= 150 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					tmpHour -= 0.5;
					sprintf(buffer, "%0.1f	hour", tmpHour);
				}
				// increment by 1/2 hour
				else if (tmpHour + 0.5 <= MAX_HOUR && coords.y >= 50 && coords.y <= 150 && coords.x >= 80 && coords.x <= 120){		// 320-200, 320-240
					tmpHour += 0.5;
					sprintf(buffer, "%0.1f	hour", tmpHour);
				}
				// finish input, move to next operation
				else if (coords.y >= 180 && coords.y <= 250 && coords.x > 0 && coords.x <= 50){			// 320-180, 320-320
					task->targetHour = tmpHour;
					saveData("targetHour", (int)(tmpHour*10));
					return;
				}	
				// invalid input
				LCD_Clear (10, 100, 240, 50, BACKGROUND);
				LCD_DrawString(100,100,buffer,DEFAULT_FONTSIZE); 
				HAL_Delay(100);
			}
		}
	}
	
	void inputDirection(windingTask* task) {
		/*
		Manually input winding direction
		*/
		int windDir;
		LCD_DrawString(30,50,"Choose winding direction",DEFAULT_FONTSIZE);
		for (int i = 0; i < 3; i++) {
			LCD_DrawRectButton(30,120+(50*i),140,40,windDirName[i], BLACK);
		}
		LCD_DrawString(170,300,"Next >>",DEFAULT_FONTSIZE);

		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				// clockwise
				if (coords.y >= 30 && coords.y <= 170 && coords.x >= 160 && coords.x <= 200) {		// 320-150, 320-190
					windDir = 0;
				}
				// anti-clockwise
				else if (coords.y >= 30 && coords.y <= 170 && coords.x >= 110 && coords.x <= 150){		// 320-200, 320-240
					windDir = 1;
				}
				// bi-direction
				else if (coords.y >= 30 && coords.y <= 170 && coords.x >= 60 && coords.x <= 100){			// 320-180, 320-320
					windDir = 2;
				}	
				// next
				else if (coords.y >= 170 && coords.y <= 240 && coords.x >= 20 && coords.x <= 60) {
					task->windDir = windDir;
					saveData("windDir", windDir);
					return;
				}
				// invalid input
				LCD_Clear (10, 70, 240, 50, BACKGROUND);
				LCD_DrawString(50,80,windDirName[windDir],DEFAULT_FONTSIZE);
				task->windDir = windDir;
				HAL_Delay(100);
			}
	}
}

	bool startNewTask(windingTask* task) {
		/*
		Input and save target rotation and winding hour of new task
		*/
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		LCD_DrawString (20, 50, "Starting new task...", DEFAULT_FONTSIZE);
		HAL_Delay(1000);
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		
		int inputType = selectInputType();
		switch(inputType) {
			case 0:			// predefined parameter
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				selectModel(task);
				break;
			case 1: 		// manual input
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				inputTurn(task);
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				inputHour(task);
				LCD_Clear (0, 0, 240, 320, BACKGROUND);
				inputDirection(task);
				break;
		}
		return true;
	}
	
	
	void rotateFullCycle(int mode) {
		/*
		Start step motor and rotate full cycle with different rotation direction
		*/
		int t = 2;
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
	

	
	void startRotate(windingTask* task) {
		/*
		Rotate 
		*/
		LCD_Clear(0,50,200,200,BACKGROUND);
		LCD_DrawString(60,50,"Winding watch...", DEFAULT_FONTSIZE);
		
		int dir = task->windDir == 2 ? 0:task->windDir;
		int tnr = task->targetNumRotation;
		int cnr = (int)loadData("curNumRotation");		
		int delayBetweenWind = (int)(float)loadData("targetHour")/10*60*60*1000.0/tnr - 6;
		delayBetweenWind = 1000;
		
		// configure progress bar
		progressBar pb;
		pb.range = tnr;
		pb.left = 18;
		pb.top = 150;
		pb.width = 204;
		pb.height = 20;
		LCD_DrawRectButton(pb.left,pb.top,pb.width,pb.height,"",BLACK);
		
		// update flash memory and progress bar after each rotation
		for (int i = 0; i < tnr; i++) {
			LCD_Clear(50, 100, 200, 20, BACKGROUND);
			while(interruptFlag) {
				LCD_Clear(50, 100, 200, 20, BACKGROUND);
				incrementInterruptTimer(1);
			}
			rotateFullCycle(dir);
			LCD_UpdatePb(&pb, i+1,BLUE);
			task->curNumRotation = i;
			if ((rand() % 2) && (int)loadData("windDir") == 2) {
				dir = !dir;
				HAL_Delay(delayBetweenWind);
			}
			if (saveData("curNumRotation", i)) break;
		}
		LCD_UpdatePb(&pb, tnr, BLUE);

	}
	
	
	void startWinding(windingTask* task) {
		/*
		Start winding process and display progress
		*/
		char buffer1[50], buffer2[50], buffer3[50];
		sprintf(buffer1, "Turn: %d", task->targetNumRotation);
		sprintf(buffer2, "Duration: %0.1f hour", (float)loadData("targetHour")/10);
		sprintf(buffer3, "Direction: %s", windDirName[task->windDir]);
		
		LCD_Clear(0,0,240,320,BACKGROUND);
		LCD_DrawString(10,50,buffer1,DEFAULT_FONTSIZE);
		LCD_DrawString(10,70,buffer2,DEFAULT_FONTSIZE);
		LCD_DrawString(10,90,buffer3,DEFAULT_FONTSIZE);
		sprintf(buffer3, "CurDuration: %0.1f hour",task->curHour);
		LCD_DrawString(10,110,buffer3,DEFAULT_FONTSIZE);

		LCD_DrawRectButton(70,150,100,50,"Start",BLACK);
		
		strType_XPT2046_Coordinate coords;
		while (true) {
			if (isTouched()) {
				XPT2046_Get_TouchedPoint(&coords, &touchPara);
				if (coords.y >= 50 && coords.y <= 200 && coords.x >= 130 && coords.x <= 170) {		// 320-150, 320-190
					startRotate(task);
					eraseAllData();
					break; 		// finish rotation
				}
			}
		}
	}

	
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	/*
	int prevRotation, prevTargetRotation;
	float prevHour, prevTargetHour;
	*/
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
	
	windingTask prevTask, curTask;
	bool prevFlag = checkOngoing(&prevTask);		// check for previous task
	bool setupReady = false;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
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
						curTask.targetNumRotation = prevTask.targetNumRotation;
						curTask.curNumRotation = prevTask.curNumRotation;
						curTask.targetHour = prevTask.targetHour;
						curTask.curHour = prevTask.curHour;
						curTask.windDir = prevTask.windDir;
						setupReady = true;
						break;
					}
					
					// start a new task
					else if (coords.y >= 130 && coords.y <= 200 && coords.x <= 170 && coords.x >= 120){		// 320-150, 320-200
						LCD_Clear (0, 0, 240, 320, BACKGROUND);
						eraseAllData();		
						LCD_DrawString(20,50,"Removing previous task...", DEFAULT_FONTSIZE);
						HAL_Delay(1000);
						setupReady = startNewTask(&curTask);
						break;
					}
				}
			}
		}
		// start a new task
		else {
			setupReady = startNewTask(&curTask);
		}
		LCD_Clear (0, 0, 240, 320, BACKGROUND);
		startWinding(&curTask);
		prevFlag = false;			// to start new task
		setupReady = false;		// to set up new task
		
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
