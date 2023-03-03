/*
 * General.c
 *
 *  Created on: Mar 2, 2023
 *      Author: Вячеслав
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
/*Определение хэндлеров задач*/
osThreadId tCalcTempHandle;//задача измерения темппературы и выставления реле
/*Определение хэндлеров очередей*/
osMessageQId qADCReadyHandle;//Очередь готовности данных с ацп
/*Определение глоальных переменных*/
uint16_t Tdata[NumOfTempData]={0};//Массив для ацп
uint8_t flagReady=0;//Флаг окончания выполнения задания (сбрасывается в 0 после успешной передачи данных по уарту)
/*
 * Бинарный поиск по массиву
 * Выходной параметр: индекс элемента массива
 * Входные параметры: Искомое значение, Массив, Размер массива
 *
 * */
uint8_t BynarySearch (float a, uint16_t ADCmass[], uint8_t n)
{
	/*Объявление вспомогательных перемнных: Крайние и средний индексы массива*/
	uint8_t low=0, high=n, middle=0;
	/*Проверка искомого значения на выход за предельные значения массива*/
    if (a>ADCmass[0]){return LowTemp;}//Если значение больше, то выставляется макрос минимальной температуры
    else if (a<ADCmass[high]){return HighTemp;}//Если значение меньше, то выставляется макрос максимальной температуры
    /*Цикл прогона по массиву методом бинарного поиска.*/
    while (low<=high)//Пока low не перескочил high
    {
        middle=(low+high)/2;//Выставление среднего индекса
        if (low==high){return low;}//Если значение low сравнялось с high то нашли искомый индекс
        /* Искомое значение может находится между двумя значениями массива.
         * Сначала проверяется находится ли искомое значение в окружности значения массива\
         * */
        else if (a<(ADCmass[middle]+ADCmass[middle-1])/2&&a>=(ADCmass[middle]+ADCmass[middle+1])/2){return middle;}//Возвращается индекс
        /* Если нет, то проверяется больше или меньше этой окружности значения*/
        else if (a>(ADCmass[middle]+ADCmass[middle-1])/2){high=middle-1;}//Если больше, то двигается переменная high
        else if (a<(ADCmass[middle]+ADCmass[middle+1])/2){low=middle+1;}//если меньше, то двигается переменная low
    }
    /*Если значение не было найдено, возвращается -1*/
    return 255;
}
/* Задача расчета и установления реле.
 * Бесконечно ожидается флага с очереди qADCReadyHandle.*/
void CalcTempTask(void const * argument)
{
	/*Объявление внутренних переменных*/
	float tdata[2]={0};//Переменная для усреднения значений ацп
	uint8_t temp[2]={0};//Переменная для температуры
	osEvent event;//Переменная для получения статуса очереди
	for (;;)
	{
		event=osMessageGet(qADCReadyHandle, osWaitForever);//Получаем состояние очереди, Если сообщения нет, то ждем бесконечно
		if (event.status==osEventMessage)//Если пришло сообщение
		{
			/*Останавливаем ацп, таймер и возвращаем таймер в исходное состояние. Необходимо для корректного повторного запуска*/
			HAL_ADC_Stop_DMA(&hadc1);
			HAL_TIM_Base_Stop(&htim3);
			TIM3->CNT=0;
			TIM3->EGR|=TIM_EGR_UG;
			/*Усреднение значений АЦП. перевод из кодов АЦП в напряжение не производится*/
			for (uint8_t i=0;i<(NumOfTempData/2);i++){tdata[0]+=Tdata[2*i];tdata[1]+=Tdata[2*i+1];}
			tdata[0]/=(NumOfTempData/2);
			tdata[1]/=(NumOfTempData/2);
			/*По каждому полученному значению проводим бинарный поиск в массиве*/
			temp[0]=BynarySearch(tdata[0],(uint16_t *)&ADCval,sizeof(ADCval)-1);
			temp[1]=BynarySearch(tdata[1],(uint16_t *)&ADCval,sizeof(ADCval)-1);
			/*Проверка полученных значений температуры по условиям для выставления реле*/
			if (temp[0]==255||temp[0]==LowTemp||temp[0]==HighTemp||temp[1]==255||temp[1]==LowTemp||temp[1]==HighTemp)//Если температура вышла за пределы
			{
				/*Выключаем оба реле*/
				FORWARDReSet();
				BACKWARDReSet();
			}
			else if (temp[0]==temp[1]||temp[0]==temp[1]-1||temp[0]==temp[1]+1)//Если значения температуры равны с dt 1 выполняем проверку
			{
				if (temp[0]<=25){BACKWARDReSet();FORWARDSet();}//Если температура ниже 25 то выключаем реле BACKWARD и включаем FORWARD
				else {FORWARDReSet();BACKWARDSet();}//Если температура выше 25 то выключаем реле FORWARD и включаем BACKWARD
			}
			else//Если температура не равна то выключаем оба реле
			{
				FORWARDReSet();
				BACKWARDReSet();
			}
			/*В заключение передачем полученные значения температуры по уарту*/
			HAL_UART_Transmit_DMA(&huart3, (uint8_t*)&temp, sizeof(temp));
		}
	}
}
void StartDefaultTask(void const * argument)
{
	/*Инициализация задач и очередей*/
	osThreadDef(tCalcTemp, CalcTempTask, osPriorityNormal, 0, 128);
	tCalcTempHandle = osThreadCreate(osThread(tCalcTemp), NULL);

	osMessageQDef(qADCReady,1,uint8_t);
	qADCReadyHandle=osMessageCreate(osMessageQ(qADCReady), NULL);
	/*Выставление значений таймера для тактирования ацп с обязательным флагом апдейта*/
	TIM3->PSC=720-1;
	TIM3->ARR=10000-1;
	TIM3->EGR|=TIM_EGR_UG;
	/*Объявление внутренних переменных*/
	uint8_t cnt=0;//Переменная для фильтрации кнопки
	uint32_t tickcount = osKernelSysTick();//Получение системного тика таймера для организации работы строго установленное время
	for (;;)
	{
		/*Проверка нажатия кнопки, счетного фильтра и флага работы задания*/
		if (ButtonBackRead()==1&&cnt<MaxCntVal&&flagReady==0)//Если все ок
		{
			cnt++;//Увеличиваем значение счетчика
			if (cnt==MaxCntVal)//Дошли до максимального значения
			{
				flagReady=1;//Выставляется флаг запуска задания
				/*Колибровка, запуск ацп и таймера*/
				HAL_ADCEx_Calibration_Start(&hadc1);
				HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&Tdata, NumOfTempData);
				HAL_TIM_Base_Start(&htim3);
			}
		}
		else if (ButtonBackRead()==0){cnt=0;}//Если пришел 0 то сбрасываем счетчик
		osDelayUntil(&tickcount, 1);
	}
}
/*Функция прерывания ацп по готовности преобразования*/
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1){osMessagePut(qADCReadyHandle,1,0);}//Если прерывание пришло с ацп1 то выставляем в очередь флаг
}
/*Функция прерывания уарта по готовности передачи*/
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart -> Instance == USART3){flagReady=0;}//Если перерывание пришло с уарт3 то обнуляем флаг работы задания
}
