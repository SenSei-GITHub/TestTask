/*
 * General.c
 *
 *  Created on: Mar 2, 2023
 *      Author: ֲÿקוסכאג
 */
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "gpio.h"
#include "adc.h"
#include "usart.h"

#include "General.h"

osThreadId tButtonReadHandle;
osThreadId tTempCountHandle;
osThreadId tSetRelayHandle;

osMessageQId qADCReadyHandle;
osMessageQId qTempReadyHandle;
#ifdef Debug
static uint32_t StackSize_tButtonRead=0;
static uint32_t StackSize_tTempCount=0;
static uint32_t StackSize_tSetRelay=0;
#endif
static uint8_t flag=0;
static uint16_t Tdata[NumOfTempData]={0},Temp[2]={0};

void ButtonReadTask(void const * argument)
{
	for(;;)
	{
		if (ButtonBackRead()==1&&flag==0)
		{
		  flag=1;
		  HAL_ADC_Start_DMA(&hadc, (uint32_t*)&Tdata, NumOfTempData);
		}
		osDelay(100);
#ifdef Debug
		StackSize_tButtonRead = uxTaskGetStackHighWaterMark(NULL);
#endif
	}
}
void TempCountTask(void const * argument)
{
	static uint16_t tdata[2]={0};
	static osEvent event;
	for(;;)
	{
		event=osMessageGet(qADCReadyHandle, osWaitForever);
		if (event.status==osEventMessage)
		{
			HAL_ADC_Stop_DMA(&hadc);
			tdata[0]=0;
			tdata[1]=0;
			for (uint8_t i=0;i<(NumOfTempData/2);i++){tdata[0]+=Tdata[2*i];tdata[1]+=Tdata[2*i+1];}
			Temp[0]=tdata[0]/(NumOfTempData/2);
			Temp[1]=tdata[1]/(NumOfTempData/2);
			osMessagePut(qTempReadyHandle,1,1000);
		}
#ifdef Debug
		StackSize_tTempCount = uxTaskGetStackHighWaterMark(NULL);
#endif
	}
}
void SetRelayTask(void const * argument)
{
	static osEvent event;
	for(;;)
	{
		event=osMessageGet(qTempReadyHandle, osWaitForever);
		if (event.status==osEventMessage)
		{
			if (Temp[0]-Temp[1]<=100)
			{
				if (Temp[0]<=2048||Temp[1]<=2048){BACKWARDReSet();FORWARDSet();}
				else if (Temp[0]>2048||Temp[1]>2048){FORWARDReSet();BACKWARDSet();}
				else {FORWARDReSet();BACKWARDReSet();}
			}
			else{FORWARDReSet();BACKWARDReSet();}
			HAL_UART_Transmit(&huart3, (uint8_t *)&Temp, sizeof(Temp), 100);
			flag=0;
			osDelay(1000);
		}
#ifdef Debug
		StackSize_tSetRelay = uxTaskGetStackHighWaterMark(NULL);
#endif
	}
}
uint8_t SysIni (void)
{
	osThreadDef(tButtonRead, ButtonReadTask, osPriorityNormal, 0, 30);
	tButtonReadHandle = osThreadCreate(osThread(tButtonRead), NULL);
	if (tButtonReadHandle==NULL)
		return 0;
	osThreadDef(tTempCount, TempCountTask, osPriorityNormal, 0, 64);
	tTempCountHandle = osThreadCreate(osThread(tTempCount), NULL);
	if (tTempCountHandle==NULL)
		return 0;
	osThreadDef(tSetRelay, SetRelayTask, osPriorityNormal, 0, 64);
	tSetRelayHandle = osThreadCreate(osThread(tSetRelay), NULL);
	if (tSetRelayHandle==NULL)
		return 0;
	osMessageQDef(qADCReady,1,uint8_t);
	qADCReadyHandle=osMessageCreate(osMessageQ(qADCReady), NULL);
	if (qADCReadyHandle==NULL)
		return 0;
	osMessageQDef(qTempReady,1,uint8_t);
	qTempReadyHandle=osMessageCreate(osMessageQ(qTempReady), NULL);
	if (qTempReadyHandle==NULL)
		return 0;

	return 1;
}
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef* hadc)
{
	osMessagePut(qADCReadyHandle,1,0);
}
