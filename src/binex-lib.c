/******************************************************************************
File:   binex-lib.c
Autor:  Sivokon Dmitriy aka DiMoon Electronics
*******************************************************************************
MIT License

Copyright (c) 2025 Sivokon Dmitriy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#include "binex-lib/public-api.h"
#include "config/binex-lib-conf.h"

/******************************************************************************/

#define BINEX_ESCAPE 0x00
#define BINEX_START 0x01
#define BINEX_CHAR 0x02
#define BINEX_INVALID 0x03


//символ начала пакета
#define BINEX_START_SYMBOL 0xF5

//esc-символ
#define BINEX_ESC_SYMBOL 0xF4

/******************************************************************************/

uint8_t binex_receive_buffer[BINEX_BUFFER_SIZE];
static uint16_t rxpack_size;
static uint8_t rxstate;
static uint8_t flag_prev_rx_esc;

static uint16_t txpack_size;
static uint8_t *txbuff;
static uint8_t txstate;
static uint8_t flag_prev_tx_esc;

#ifdef BINEX_CHECK_CRC
static uint16_t tx_crc16;
#endif

/******************************************************************************/

static uint8_t char_rx(uint8_t c)
{
  if (flag_prev_rx_esc)
  {
    // Если предыдущий символ является esc-символом
    // Сбрасываем флаг, так как
    // оба сценария развития событий
    // предусматирвают сброс этого флага

    flag_prev_rx_esc = 0;

    // Если пришла экранированная последовательность
    if ((c == BINEX_ESC_SYMBOL) || (c == BINEX_START_SYMBOL))
      return BINEX_CHAR;

    // Если предыдущее условие не выполнилось,
    // то получили некорректную последовательность
    return BINEX_INVALID;
  }

  // Если попали сюда, то это означает, что
  // прошлый раз был принят не esc-символ
  if (c == BINEX_ESC_SYMBOL)
  {
    // Если приняли esc-символ,
    // то устанавливаем соответствующий флаг
    flag_prev_rx_esc = 1;

    // Просто выходим
    return BINEX_ESCAPE;
  }

  // Если пришел стартовый символ
  if (c == BINEX_START_SYMBOL)
    return BINEX_START;

  // Если дошли сюда, то это означает, что приняли
  // обычный символ, возвращаем его
  return BINEX_CHAR;
}

static int char_tx(uint8_t c)
{
  // Если экранирование esc-символа
  if (flag_prev_tx_esc)
  {
    if (binex_tx_callback(c))
    {
      // На предыдущем шаге отправили esc-символ,
      // сейчас сам символ, поэтому возвращаем 1
      flag_prev_tx_esc = 0;
      return 1;
    }
    return 0;
  }

  if ((c == BINEX_ESC_SYMBOL) || (c == BINEX_START_SYMBOL))
  {
    if (binex_tx_callback(BINEX_ESC_SYMBOL))
    {
      flag_prev_tx_esc = 1;
    }

    // Либо ничего не отправили, либо отправили esc-символ,
    // поэтому возвращаем 0
    return 0;
  }

  if (binex_tx_callback(c))
    return 1;

  return 0;
}

/******************************************************************************/

void binex_receiver_reset(void)
{
  rxstate = 0;
  flag_prev_rx_esc = 0;
  rxpack_size = 0;
}

BinexRxStatus_t binex_receiver(int16_t c)
{
  static uint16_t tmp;
  uint8_t r;

  if (c < 0)
    return BINEX_PACK_NOT_RX;

  switch (rxstate)
  {
  case 0:
    // Начало приема пакета
    // Ожидается символ начала пакета
    if (char_rx(c) == BINEX_START)
    {
      rxstate = 1;
    }
    break;
  //////////////////////////////////////
  case 1:
    /// Прием 1го байта заголовка пакета ///

    r = char_rx((uint8_t)c);
    if (r == BINEX_CHAR)
    {
      // приняли символ
      rxpack_size = ((uint8_t)c);
      rxstate = 2;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      // Если приняли неожиданное начало пакета
      // либо некорректную esc-последовательность
      // переходим в состояние приема старта пакета
      // и возвращаем ошибку
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 2:
    /// Прием 2го байта заголовка пакета ///

    r = char_rx((uint8_t)c);
    if (r == BINEX_CHAR)
    {
      rxpack_size |= (((uint8_t)c) << 8);
      tmp = 0;
      rxstate = 3;

      if (rxpack_size > BINEX_BUFFER_SIZE)
      {
        // Пакет слишком большой.
        // Это могло произойти по 2м причинам:
        // 1. пакет действительно слишком большой
        // 2. возникла ошибка при приеме размера пакета
        rxstate = 0;
        return BINEX_PACK_BROKEN;
      }
      else if (rxpack_size == 0) // Пустой пакет
      {
#ifdef BINEX_CHECK_CRC
        // Состояние получения CRC16
        rxstate = 4;
#else
        // Если crc не проверяем, то
        // возвращаемся в состояние приема
        // начала пакета, и возвращаем информацию о том,
        // что пакет был принят
        rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 3:
    /// Прием тела пакета ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      // приняли символ
      binex_receive_buffer[tmp++] = (uint8_t)c;
      if (tmp == rxpack_size) // приняли весь пакет
      {
#ifdef BINEX_CHECK_CRC
        // Если crc проверяем
        rxstate = 4; // прием и проверка CRC
#else
        // если crc не проверяем
        rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
    //////////////////////////////////////
#ifdef BINEX_CHECK_CRC
  case 4:
    /// Получение 1го байта crc ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      tmp = ((uint8_t)c);
      rxstate = 5;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 5:
    /// Получение 2го байта crc ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      tmp |= ((uint8_t)c) << 8;
      rxstate = 0;

      // Вычисление crc для поля длины пакета
      // и поля полезных данных
      uint16_t crc = binex_crc16_start_value();
      crc = binex_crc16_calc((uint8_t *)((void *)(&rxpack_size)), 2, crc);
      crc = binex_crc16_calc(binex_receive_buffer, rxpack_size, crc);

      if (crc == tmp)
        return BINEX_PACK_RX;
      else
        return BINEX_PACK_BROKEN;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
#endif
  }

  return BINEX_PACK_NOT_RX;
}

uint16_t binex_get_rxpack_len(void)
{
  return rxpack_size;
}

void binex_transmitter_init(void *buff, uint16_t size)
{
  txbuff = (uint8_t *)buff;
  txpack_size = size;
  txstate = 0;
  flag_prev_tx_esc = 0;
#ifdef BINEX_CHECK_CRC
  tx_crc16 = binex_crc16_start_value();
  tx_crc16 = binex_crc16_calc((uint8_t *)((void *)(&txpack_size)), 2, tx_crc16);
  tx_crc16 = binex_crc16_calc(txbuff, size, tx_crc16);
#endif
}

BinexTxStatus_t binex_transmit(void)
{
  static uint16_t tmp;

  for (;;)
  {
    switch (txstate)
    {
    case 0:
      /// Начало процесса передачи ///
      if (binex_tx_callback((uint8_t)BINEX_START_SYMBOL))
      {
        tmp = txpack_size;
        txstate = 1;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 1:
      /// Передача 1го байта длины пакета ///
      if (char_tx((uint8_t)tmp))
      {
        tmp >>= 8;
        txstate = 2;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 2:
      /// Передача 2го байта длины пакета ///
      if (char_tx((uint8_t)tmp))
      {
        tmp = 0;
        txstate = 3;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 3:
      /// Передача тела пакета ///
      // Если все передали
      if (tmp >= txpack_size)
      {
#ifdef BINEX_CHECK_CRC
        txstate = 4;
        break;
#else
        txstate = 6;
        return BINEX_PACK_TX;
#endif
      }

      if (char_tx(txbuff[tmp]))
        tmp++;
      else
        return BINEX_PACK_NOT_TX;
      break;
      /////////////////////////////////////////////
#ifdef BINEX_CHECK_CRC
    case 4:
      /// Передача 1го байта CRC ///
      if (char_tx((uint8_t)tx_crc16))
      {
        tx_crc16 >>= 8;
        txstate = 5;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 5:
      /// Передача 2го байта CRC ///
      if (char_tx((uint8_t)tx_crc16))
      {
        txstate = 6;
        return BINEX_PACK_TX;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
#endif
    /////////////////////////////////////////////
    default:
      return BINEX_PACK_TX;
    }
  }
}
