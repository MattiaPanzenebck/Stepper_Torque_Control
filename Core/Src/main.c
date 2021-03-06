/* USER CODE BEGIN Header */
/**       28-4
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
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
#include <math.h>
#include <string.h>
#include <stdio.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim10;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define pi 3.14159   //pi constant
#define presc 1
int kk;
int j, flag, send;
uint64_t aa, b, ii, qq;
float delta, dt=900000000.0, a, preva, vmax = 30.0, vmin = 0.0;
#define tau 15.0             //belt transmission ratio (R/r)
float stepsperrev = 800.0;   //steps per revolution as set on the driver

#define alpha2 0.05
#define alphay 0.01
#define alphavy 0.01
#define alphax 0.05
#define alphavenc 1.0
#define alphav 0.9
#define alphaderr 0.2


//#define f1 146.0968
//#define f2 -256.369
//#define f3 114.2722

float ctrt, err, ecum, preverr, derr, derraux;
float y, prevy, yo = -0.7, vy, vyaux, vyo=0.0,  ym, in1aux, unf, in1, in2, v, vaux, inc, p0;
float z, kp=15.0, kd=0.2;  //kp is torque loop prop gain, kd is derivative

float kv = 50.0, cv = 0.0;            //desired virtual stiffness [N*m/rad], desired virtual damping [N*m*s/rad]

#define tsamp 120.0    //sampling window duration [s]
#define fc 1000.0     //control frequency [Hz]    !!! Has to be a divisor of 20000 !!!
#define r 4          //ratio between control frequency and sampling frequency !!! Has to be a divisor of fc !!!
#define fs fc/r       //sampling frequency
#define nsamp (int)(tsamp*fs)  //number of samples that will be acquired and sent via usart
int option = 0;       //case to be executed: 0=impedance control; 1= speed imposition, 2= torque control
uint16_t rawenc;
float rawfl;
float penc, prevpenc, venc, vencaux;

float fm2, fm1, unfm1, unfm2;

//float out1[nsamp+1], out2[nsamp+1];   //output variables
char out11[20], out22[20];            //useful char arrays for usart

GPIO_PinState button, prevButton;   //button states

//torque, ref torque, ref torque signal parameters,
float c, cr, cra = 0.8, crm = 1.5, cmax = 4.0;

//chirp parameters
float fstart = 0.1, deltaf = 19.9;

uint16_t raw;  //raw signal auxiliary variable
int aaa;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM10_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
static void runSpeed(float dt);
static void ADC_Select_CH0 (void);
static void ADC_Select_CH1 (void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

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
  MX_ADC1_Init();
  MX_USART2_UART_Init();
  MX_TIM10_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim3);  //start step timer
  HAL_TIM_Base_Start_IT(&htim10);  //start interrupt timer
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_1|TIM_CHANNEL_2);  //start encoder timer

    a = __HAL_TIM_GET_COUNTER(&htim3);

    //initialize filter values (assign the raw value to the first filtered value as a first "guess"):
        	  ADC_Select_CH0();
              HAL_ADC_Start(&hadc1);
              HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
              raw = HAL_ADC_GetValue(&hadc1);
              HAL_ADC_Stop(&hadc1);
              in1 = (float)raw;
              p0 = 0; //(in1-1573.0)/1010.6;
              y = p0;

              ADC_Select_CH1();
              HAL_ADC_Start(&hadc1);
              HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
              raw = HAL_ADC_GetValue(&hadc1);
              HAL_ADC_Stop(&hadc1);
              in2 = (float)raw;
              unf = in2; fm2 = in2; fm1 = in2; unfm1= in2; unfm2= in2;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  runSpeed(dt);

	  //send out selected variables via usart
	  if(send){
		  if(flag%r==0){
			  //sprintf(out22, "%3f\t",((float)in1/4096.0*3.3-2.5)*4.0*5.65/5.0);
			  //sprintf(out22, "%3f\t",(float)in1/4096.0*3.3);
			  //sprintf(out22, "%4f\t",cr);
			  sprintf(out22, "%3f\t",((float)in2-2224.0)*-0.008);
			  HAL_UART_Transmit(&huart2, out22, strlen((char*)out22), HAL_MAX_DELAY);
		  }
		  else if(flag%r==r/2){
			  //sprintf(out22, "%3f\r\n",(float)in1/4096.0*3.3);
			  //sprintf(out22, "%3f\r\n",((float)in2-2224.0)*-0.008);
			  //sprintf(out22, "%3f\r\n",penc*10.0);
			  //sprintf(out22, "%4f\r\n",yo-y);
			  sprintf(out22, "%3f\r\n",vy);
			  //sprintf(out22, "%4f\r\n",cr);
			  HAL_UART_Transmit(&huart2, out22, strlen((char*)out22), HAL_MAX_DELAY);
		  }
		  send=0;
	  }


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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLRCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 10;
  if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = presc-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM10 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM10_Init(void)
{

  /* USER CODE BEGIN TIM10_Init 0 */

  /* USER CODE END TIM10_Init 0 */

  /* USER CODE BEGIN TIM10_Init 1 */

  /* USER CODE END TIM10_Init 1 */
  htim10.Instance = TIM10;
  htim10.Init.Prescaler =  9000-1;
  htim10.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim10.Init.Period = 20000.0/fc-1;
  htim10.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim10.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim10) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM10_Init 2 */

  /* USER CODE END TIM10_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 1000000;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5 PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)   //function called by the interrupt (sample-update routine)
{ qq++;

	//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
switch (option)
{ qq++;

case 0:
//impedance control /////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//button pressing
		    button = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
		    if ((button!=prevButton)&&(button==1)){aaa=!aaa;}
		    prevButton = button;

	//do everything only if button has been pressed
		    if(!aaa){

	//read position channel from ADC and update in1aux
		    ADC_Select_CH0();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
	        in1aux = (float)raw;

	//read POT channel from ADC and update in2aux
	        ADC_Select_CH1();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
            unf = (float)raw;

    //read position from encoder
           rawenc = TIM1->CNT;
           if (rawenc>40000) {penc = p0+((rawenc-65536)/4000.0/tau*2*pi);}
           else {penc = p0+(rawenc/4000.0/tau*2*pi);}

	 //filtering
	        in1 = in1aux*alphay + in1*(1-alphay);
	        in2 = unf*alpha2 + in2*(1-alpha2);
	        //in2 = (unf + 2.0*unfm1 + unfm2 - f2*fm1 - f3*fm2)/f1;
	        //unfm1 = unf;
	        //unfm2 = unfm1;
	        //fm1 = in2;
	        //fm2 = fm1;

	//conversion to physical units
	        y = (in1-1573.0)/1010.6;        //y angle [rad]
	        vyaux = (y-prevy)*fc;              //y velocity [rad/s]
	        vy = vyaux*alphavy + vy*(1-alphavy);
	        prevy = y;

	        c = -((float)in2-2224.0)*0.008;                        //torque read from POT[Nm]

	       if((c>=cmax)||(c<=-1.0*cmax)) {HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);} //limit torque

   //torque reference

            cr = kv*(yo-y) + cv*(vyo-vy);

	 //torque control: compute speed on base of torque error and its derivative
            err = cr-c;
            derraux = (err-preverr)*fc; //torque error derivative
            derr = derraux*alphaderr + (1-alphaderr)*derr;
            preverr = err;
            if((err<0.2)&&(err>-0.0)) {v=0.0;}  //error dead band
            else {v = kp*err + kd*derr;}     //speed in rad/s at motor side (x DOF)

            if(v>=vmax) {v = vmax;}                         //to deal with saturation
            else if(v<=-1.0*vmax) {v=-1.0*vmax;}            //to deal with saturation
            else if((v>=-1.0*vmin)&&(v<=vmin)) {v = 0.0;}   //to avoid vibrations (speed dead-band)


//read position from encoder
	        //rawenc = TIM1->CNT;
	        //rawfl = (float)rawenc;
	        //if (rawenc>40000) {penc = p0+((rawenc-65536)/4000.0/tau*2*pi);}
	        //else {penc = p0+(rawenc/4000.0/tau*2*pi);}

	        //compute speed from encoder and filter it [rad/s]
	        //vencaux = (penc-prevpenc)*fc;
	        //venc = vencaux*alphavenc + venc*(1-alphavenc);
	        //prevpenc = penc;

	        //compute step time interval in clock pulses, terms in brackets are needed to pass from rad/s to steps/s
			dt = 90000000.0/(v/2.0/pi*stepsperrev);

	//update routine counter
			kk++;

			flag++; //flag on the data sending
			send = 1;
			if(kk>fc*tsamp){send = 0; v = 0;}

		    }
	break;

case 1:
//speed imposition ///////////////////////////////////////////////////////////////////////////////////////////////////////////
	//button pressing
		    button = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
		    if ((button!=prevButton)&&(button==1)){aaa=!aaa;}
		    prevButton = button;

	//do everything only if button has been pressed
		    if(!aaa){

	//read position channel from ADC and update in1aux
		    ADC_Select_CH0();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
	        in1aux = (float)raw;

	//read torque channel from ADC and update in2aux
	        ADC_Select_CH1();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
            unf = (float)raw;

	 //filtering
	        in1 = in1aux*alphay + in1*(1-alphay);
	        in2 = unf*alphay + in2*(1-alphay);
	        //in2 = (unf + 2.0*unfm1 + unfm2 - f2*fm1 - f3*fm2)/f1;
	        //unfm1 = unf;
	        //unfm2 = unfm1;
	        //fm1 = in2;
	        //fm2 = fm1;

	//conversion to physical units
	        y = (in1-1573.0)/1010.6;        //y angle [rad]
	        //d = (in2-2230.0)/15168.0;       //relative angle [rad]
	        //c = -d*130.5;                   //torque [Nm]
	        if((c>=cmax)||(c<=-1.0*cmax)) {HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);} //limit torque
	        z = y;                       //z angle [rad]

	//speed signal generation [rad/s]
            //v = 10.0;
	        //v =  cra*sin(2*pi*(1/fc*kk*fstart + deltaf/2.0/tsamp*kk/fc*kk/fc));   //chirp
	        //v = cra*sin(2*pi*0.5*kk/fc);
	        //if(kk<=500) v = 0.08*kk;
	        //else {v = 40-0.08*(kk-500);}
	        //v = 0.002*kk;
            //v = 1.0*v;

	        //if(v>=vmax) {v = vmax;}                         //to deal with saturation
	        //else if(v<=-1.0*vmax) {v=-1.0*vmax;}            //to deal with saturation
	        //else if((v>=-1.0*vmin)&&(v<=vmin)) {v = 0.0;}   //to avoid vibrations (speed dead-band)

//read position from encoder
	        rawenc = TIM1->CNT;
	        rawfl = (float)rawenc;
	        if (rawenc>40000) {penc = p0+((rawenc-65536)/4000.0/tau*2*pi);}
	        else {penc = p0+(rawenc/4000.0/tau*2*pi);}

	        //compute speed from encoder and filter it [rad/s]
	        vencaux = (penc-prevpenc)*fc;
	        venc = vencaux*alphavenc + venc*(1-alphavenc);
	        prevpenc = penc;

//save into output variables
	        //out1[kk/r] = v;
	        //out2[kk/r] = venc*tau;

	        //compute step time interval in clock pulses, terms in brackets are needed to pass from rad/s to steps/s
			dt = 90000000.0/(v/2.0/pi*stepsperrev);

	//update routine counter
			kk++;

			flag++; //flag on the data sending
			send = 1;

			//send the output arrays via usart to PC if test has ended
					if(kk>fc*tsamp){send = 0; v = 0;}
						//for(int uu=1; uu<=nsamp; uu++){
						  //sprintf(out11, "%.4f,",out1[uu]);
						  //HAL_UART_Transmit(&huart2, out11, strlen((char*)out11), HAL_MAX_DELAY);
						  //sprintf(out22, "%.3f\r\n",venc);
						  //HAL_UART_Transmit(&huart2, out22, strlen((char*)out22), HAL_MAX_DELAY);
						  //}  HAL_UART_Transmit(&huart2, "a", 1, HAL_MAX_DELAY);
					    //}
		    }
	break;

case 2:
//torque control/////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//button pressing
		    button = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
		    if ((button!=prevButton)&&(button==1)){aaa=!aaa;}
		    prevButton = button;

	//do everything only if button has been pressed
		    if(!aaa){

	//read TRT channel from ADC and update in1aux
		    ADC_Select_CH0();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
	        in1aux = (float)raw;

	//read POT channel from ADC and update in2aux
	        ADC_Select_CH1();
	        HAL_ADC_Start(&hadc1);
	        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	        raw = HAL_ADC_GetValue(&hadc1);
	        HAL_ADC_Stop(&hadc1);
            unf = (float)raw;

    //read position from encoder
           rawenc = TIM1->CNT;
           if (rawenc>40000) {penc = p0+((rawenc-65536)/4000.0/tau*2*pi);}
           else {penc = p0+(rawenc/4000.0/tau*2*pi);}

	 //filtering
	        in1 = in1aux*alphax + in1*(1-alphax);
	        in2 = unf*alpha2 + in2*(1-alpha2);
	        //in2 = (unf + 2.0*unfm1 + unfm2 - f2*fm1 - f3*fm2)/f1;
	        //unfm1 = unf;
	        //unfm2 = unfm1;
	        //fm1 = in2;
	        //fm2 = fm1;

	//conversion to physical units
	        //y = (in1-1573.0)/1010.6;        //y angle [rad]
	        //d = (in2-2230.0)/15168.0;       //relative angle [rad]
	        ctrt = (in1/4096.0*3.3-2.5)*4.0*5.65/5.0;  //torque read from TRT[Nm]
	        c = -((float)in2-2224.0)*0.008;                        //torque read from POT[Nm]

	       if((c>=cmax)||(c<=-1.0*cmax)) {HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);} //limit torque

   //torque reference: uncomment to have direct torque control
	        //cr =  crm + cra*sin(2*pi*(1/fc*kk*fstart + deltaf/2.0/tsamp*kk/fc*kk/fc));  //chirp
            cr = 0.0;   //static reference
            /*if(kk<4000) {cr = -0.5;}
            else if ((kk>=4000)&&(kk<8000)) {cr = -2.0;}
            else if ((kk>=8000)&&(kk<12000)) {cr = -0.5;}
            else if ((kk>=12000)&&(kk<16000)) {cr = -2.0;}
            else if ((kk>=16000)&&(kk<20000)) {cr = -0.5;}
            else if ((kk>=20000)&&(kk<24000)) {cr = -2.0;}
            else if ((kk>=24000)&&(kk<28000)) {cr = -0.5;}
            else if ((kk>=28000)&&(kk<32000)) {cr = -2.0;}
            else {cr = -0.5;}*/

    //impedance control: compute torque reference based on position error (uncomment to have imp. control)
            //..........

	 //torque control: compute speed on base of torque error and its derivative
            err = cr-c;
            derraux = (err-preverr)*fc; //torque error derivative
            derr = derraux*alphaderr + (1-alphaderr)*derr;
            preverr = err;
            if((err<0.2)&&(err>-0.2)) {v=0.0;}  //error dead band
            else {v = kp*err + kd*derr;}     //speed in rad/s at motor side (x DOF)
            //vaux = kp*err + kd*derr;
            //v = vaux*alphav + (1-alphav)*v;


            if(v>=vmax) {v = vmax;}                         //to deal with saturation
            else if(v<=-1.0*vmax) {v=-1.0*vmax;}            //to deal with saturation
            else if((v>=-1.0*vmin)&&(v<=vmin)) {v = 0.0;}   //to avoid vibrations (speed dead-band)


//read position from encoder
	        //rawenc = TIM1->CNT;
	        //rawfl = (float)rawenc;
	        //if (rawenc>40000) {penc = p0+((rawenc-65536)/4000.0/tau*2*pi);}
	        //else {penc = p0+(rawenc/4000.0/tau*2*pi);}

	        //compute speed from encoder and filter it [rad/s]
	        //vencaux = (penc-prevpenc)*fc;
	        //venc = vencaux*alphavenc + venc*(1-alphavenc);
	        //prevpenc = penc;

	        //compute step time interval in clock pulses, terms in brackets are needed to pass from rad/s to steps/s
			dt = 90000000.0/(v/2.0/pi*stepsperrev);

	//update routine counter
			kk++;

			flag++; //flag on the data sending
			send = 1;
			if(kk>fc*tsamp){send = 0; v = 0;}

		    }
	break;
} //close switch


}

void runSpeed(float dt)     //function to take (half) a step
{
	        a = __HAL_TIM_GET_COUNTER(&htim3);
	        if (a<preva) {ii++;}

	        aa = 65536*ii + a;
	        preva = a;
		  	delta = aa-b;                        //time since last (half) step

		  	if (dt<0.0){HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET); dt = -1.0*dt; inc = -0.5;}
		  	else{HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET); inc = 0.5;}

		  	  	   	if (delta >= dt/2.0)
		  	  	   	{
		  	  	   		HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);    //take (half) a step if it is due
		  	  	   	    b = aa;
		  	  	   	    //xsteps = xsteps + inc;            //update position estimated with steps
		  	  	   	}


}

void ADC_Select_CH0 (void)
{
	ADC_ChannelConfTypeDef sConfig = {0};
	  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	  */
	  sConfig.Channel = ADC_CHANNEL_0;
	  sConfig.Rank = 1;
	  sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES;
	  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	  {
	    Error_Handler();
	  }
}

void ADC_Select_CH1 (void)
{
	ADC_ChannelConfTypeDef sConfig = {0};
	  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	  */
	  sConfig.Channel = ADC_CHANNEL_1;
	  sConfig.Rank = 1;
	  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
	  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	  {
	    Error_Handler();
	  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
