/*
 * General.c
 *
 *  Created on: Mar 2, 2023
 *      Author: ��������
 */
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "gpio.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"

#include "General.h"
#include "NTC.h"
/*����������� ��������� �����*/
osThreadId tCalcTempHandle;//������ ��������� ������������ � ����������� ����
/*����������� ��������� ��������*/
osMessageQId qADCReadyHandle;//������� ���������� ������ � ���
/*����������� ��������� ����������*/
uint16_t Tdata[NumOfTempData]={0};//������ ��� ���
uint8_t flagReady=0;//���� ��������� ���������� ������� (������������ � 0 ����� �������� �������� ������ �� �����)
/*
 * �������� ����� �� �������
 * �������� ��������: ������ �������� �������
 * ������� ���������: ������� ��������, ������, ������ �������
 *
 * */
uint8_t BynarySearch (float a, uint16_t ADCmass[], uint8_t n)
{
	/*���������� ��������������� ���������: ������� � ������� ������� �������*/
	uint8_t low=0, high=n, middle=0;
	/*�������� �������� �������� �� ����� �� ���������� �������� �������*/
    if (a>ADCmass[0]){return LowTemp;}//���� �������� ������, �� ������������ ������ ����������� �����������
    else if (a<ADCmass[high]){return HighTemp;}//���� �������� ������, �� ������������ ������ ������������ �����������
    /*���� ������� �� ������� ������� ��������� ������.*/
    while (low<=high)//���� low �� ���������� high
    {
        middle=(low+high)/2;//����������� �������� �������
        if (low==high){return low;}//���� �������� low ���������� � high �� ����� ������� ������
        /* ������� �������� ����� ��������� ����� ����� ���������� �������.
         * ������� ����������� ��������� �� ������� �������� � ���������� �������� �������\
         * */
        else if (a<(ADCmass[middle]+ADCmass[middle-1])/2&&a>=(ADCmass[middle]+ADCmass[middle+1])/2){return middle;}//������������ ������
        /* ���� ���, �� ����������� ������ ��� ������ ���� ���������� ��������*/
        else if (a>(ADCmass[middle]+ADCmass[middle-1])/2){high=middle-1;}//���� ������, �� ��������� ���������� high
        else if (a<(ADCmass[middle]+ADCmass[middle+1])/2){low=middle+1;}//���� ������, �� ��������� ���������� low
    }
    /*���� �������� �� ���� �������, ������������ -1*/
    return 255;
}
/* ������ ������� � ������������ ����.
 * ���������� ��������� ����� � ������� qADCReadyHandle.*/
void CalcTempTask(void const * argument)
{
	/*���������� ���������� ����������*/
	float tdata[2]={0};//���������� ��� ���������� �������� ���
	uint8_t temp[2]={0};//���������� ��� �����������
	osEvent event;//���������� ��� ��������� ������� �������
	for (;;)
	{
		event=osMessageGet(qADCReadyHandle, osWaitForever);//�������� ��������� �������, ���� ��������� ���, �� ���� ����������
		if (event.status==osEventMessage)//���� ������ ���������
		{
			/*������������� ���, ������ � ���������� ������ � �������� ���������. ���������� ��� ����������� ���������� �������*/
			HAL_ADC_Stop_DMA(&hadc1);
			HAL_TIM_Base_Stop(&htim3);
			TIM3->CNT=0;
			TIM3->EGR|=TIM_EGR_UG;
			/*���������� �������� ���. ������� �� ����� ��� � ���������� �� ������������*/
			for (uint8_t i=0;i<(NumOfTempData/2);i++){tdata[0]+=Tdata[2*i];tdata[1]+=Tdata[2*i+1];}
			tdata[0]/=(NumOfTempData/2);
			tdata[1]/=(NumOfTempData/2);
			/*�� ������� ����������� �������� �������� �������� ����� � �������*/
			temp[0]=BynarySearch(tdata[0],(uint16_t *)&ADCval,sizeof(ADCval)-1);
			temp[1]=BynarySearch(tdata[1],(uint16_t *)&ADCval,sizeof(ADCval)-1);
			/*�������� ���������� �������� ����������� �� �������� ��� ����������� ����*/
			if (temp[0]==255||temp[0]==LowTemp||temp[0]==HighTemp||temp[1]==255||temp[1]==LowTemp||temp[1]==HighTemp)//���� ����������� ����� �� �������
			{
				/*��������� ��� ����*/
				FORWARDReSet();
				BACKWARDReSet();
			}
			else if (temp[0]==temp[1]||temp[0]==temp[1]-1||temp[0]==temp[1]+1)//���� �������� ����������� ����� � dt 1 ��������� ��������
			{
				if (temp[0]<=25){BACKWARDReSet();FORWARDSet();}//���� ����������� ���� 25 �� ��������� ���� BACKWARD � �������� FORWARD
				else {FORWARDReSet();BACKWARDSet();}//���� ����������� ���� 25 �� ��������� ���� FORWARD � �������� BACKWARD
			}
			else//���� ����������� �� ����� �� ��������� ��� ����
			{
				FORWARDReSet();
				BACKWARDReSet();
			}
			/*� ���������� ��������� ���������� �������� ����������� �� �����*/
			HAL_UART_Transmit_DMA(&huart3, (uint8_t*)&temp, sizeof(temp));
		}
	}
}
void StartDefaultTask(void const * argument)
{
	/*������������� ����� � ��������*/
	osThreadDef(tCalcTemp, CalcTempTask, osPriorityNormal, 0, 128);
	tCalcTempHandle = osThreadCreate(osThread(tCalcTemp), NULL);

	osMessageQDef(qADCReady,1,uint8_t);
	qADCReadyHandle=osMessageCreate(osMessageQ(qADCReady), NULL);
	/*����������� �������� ������� ��� ������������ ��� � ������������ ������ �������*/
	TIM3->PSC=720-1;
	TIM3->ARR=10000-1;
	TIM3->EGR|=TIM_EGR_UG;
	/*���������� ���������� ����������*/
	uint8_t cnt=0;//���������� ��� ���������� ������
	uint32_t tickcount = osKernelSysTick();//��������� ���������� ���� ������� ��� ����������� ������ ������ ������������� �����
	for (;;)
	{
		/*�������� ������� ������, �������� ������� � ����� ������ �������*/
		if (ButtonBackRead()==1&&cnt<MaxCntVal&&flagReady==0)//���� ��� ��
		{
			cnt++;//����������� �������� ��������
			if (cnt==MaxCntVal)//����� �� ������������� ��������
			{
				flagReady=1;//������������ ���� ������� �������
				/*����������, ������ ��� � �������*/
				HAL_ADCEx_Calibration_Start(&hadc1);
				HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&Tdata, NumOfTempData);
				HAL_TIM_Base_Start(&htim3);
			}
		}
		else if (ButtonBackRead()==0){cnt=0;}//���� ������ 0 �� ���������� �������
		osDelayUntil(&tickcount, 1);
	}
}
/*������� ���������� ��� �� ���������� ��������������*/
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1){osMessagePut(qADCReadyHandle,1,0);}//���� ���������� ������ � ���1 �� ���������� � ������� ����
}
/*������� ���������� ����� �� ���������� ��������*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart -> Instance == USART3){flagReady=0;}//���� ����������� ������ � ����3 �� �������� ���� ������ �������
}
