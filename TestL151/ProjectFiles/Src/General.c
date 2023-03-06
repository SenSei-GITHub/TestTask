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

static uint8_t flag=0;
static uint16_t Tdata[NumOfTempData]={0},Temp[2]={0};

void ButtonReadTask(void const * argument)
{
	for(;;)
	{
		if (ButtonBackRead()==0&&flag==0)
		{
		  flag=1;
		  HAL_ADC_Start_DMA(&hadc, (uint32_t *)&Tdata, NumOfTempData);
		}
		osDelay(100);
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
	}
}
void SetRelayTask(void const * argument)
{
	static uint8_t RelayState=0;
	static osEvent event;
	for(;;)
	{
		event=osMessageGet(qTempReadyHandle, osWaitForever);
		if (event.status==osEventMessage)
		{
			if (Temp[0]-Temp[1]<=100)
			{
				if (Temp[0]<=2048||Temp[1]<=2048){if (RelayState!=1){BACKWARDReSet();osDelay(1000);FORWARDSet();RelayState=1;}}
				else if (Temp[0]>2048||Temp[1]>2048){if (RelayState!=2){FORWARDReSet();osDelay(1000);BACKWARDSet();RelayState=2;}}
			}
			else{FORWARDReSet();BACKWARDReSet();RelayState=0;}
			HAL_UART_Transmit(&huart3, (uint8_t *)&Temp, sizeof(Temp), 100);
			flag=0;
			osDelay(1000);
		}
	}
}
uint8_t SysIni (void)
{
	osMessageQDef(qADCReady,1,uint8_t);
	qADCReadyHandle=osMessageCreate(osMessageQ(qADCReady), NULL);
	if (qADCReadyHandle==NULL)
		return 0;
	osMessageQDef(qTempReady,1,uint8_t);
	qTempReadyHandle=osMessageCreate(osMessageQ(qTempReady), NULL);
	if (qTempReadyHandle==NULL)
		return 0;
	osThreadDef(tButtonRead, ButtonReadTask, osPriorityNormal, 0, 30);
	tButtonReadHandle = osThreadCreate(osThread(tButtonRead), NULL);
	if (tButtonReadHandle==NULL)
		return 0;
	osThreadDef(tTempCount, TempCountTask, osPriorityAboveNormal, 0, 64);
	tTempCountHandle = osThreadCreate(osThread(tTempCount), NULL);
	if (tTempCountHandle==NULL)
		return 0;
	osThreadDef(tSetRelay, SetRelayTask, osPriorityHigh, 0, 64);
	tSetRelayHandle = osThreadCreate(osThread(tSetRelay), NULL);
	if (tSetRelayHandle==NULL)
		return 0;

	return 1;
}
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef* hadc)
{
	osMessagePut(qADCReadyHandle,1,0);
}
