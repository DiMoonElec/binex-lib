#ifndef __BINEXLIB_CONF_H__
#define __BINEXLIB_CONF_H__

#include <stdint.h>

#include "crc16.h"

//Если надо использовать CRC, то
//необходимо объявить define BINEX_CHECK_CRC
#define BINEX_CHECK_CRC

//Размер буфера приемника.
//Устанавливает размер чистых данных
#define BINEX_BUFFER_SIZE 192


//Обвертки для API библиотеки вычисления crc16

//Начальное состояние crc16
static inline uint16_t binex_crc16_start_value(void)
{
  return Crc16StartValue();
}

//Вычисление crc16
static inline uint16_t binex_crc16_calc(uint8_t *pcBlock, uint16_t len, uint16_t start_crc)
{
  return Crc16(pcBlock, len, start_crc);
}

#endif
