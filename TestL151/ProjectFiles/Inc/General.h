/*
 * General.h
 *
 *  Created on: Mar 2, 2023
 *      Author: ��������
 */

#ifndef INC_GENERAL_H_
#define INC_GENERAL_H_

#include "stm32l1xx_hal.h"

//#define Debug
/*������ ���������� ������� ������*/
#define ButtonBackRead() HAL_GPIO_ReadPin(Button_BACK_GPIO_Port,Button_BACK_Pin)
/*������� ��������� ���������� ����*/
#define FORWARDSet() HAL_GPIO_WritePin(AUGER_FORWARD_GPIO_Port, AUGER_FORWARD_Pin, GPIO_PIN_SET)
#define FORWARDReSet() HAL_GPIO_WritePin(AUGER_FORWARD_GPIO_Port, AUGER_FORWARD_Pin, GPIO_PIN_RESET)
#define BACKWARDSet() HAL_GPIO_WritePin(AUGER_BACKWARD_GPIO_Port, AUGER_BACKWARD_Pin, GPIO_PIN_SET)
#define BACKWARDReSet() HAL_GPIO_WritePin(AUGER_BACKWARD_GPIO_Port, AUGER_BACKWARD_Pin, GPIO_PIN_RESET)
/*������ �������*/
#define NumOfTempData 6 //������ ������� ������ � ��� ������� 2 (� ��� �������� �������� � 2 �������)

uint8_t SysIni (void);

#endif /* INC_GENERAL_H_ */
