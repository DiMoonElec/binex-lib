# binex-lib
Protocol for packet data exchange over symbolic communication interfaces.  
Библиотека пакетного обмена данными через посимвольные протоколы связи, например, через UART.  
Для работы библиотеки необходимо подключить public-api.h по пути binex-lib/public-api.h, и binex-lib-conf.h по пути config/binex-lib-conf.h. Шаблон binex-lib-conf.h можно взять из каталога templates.  
В исходном тексте программы необходимо объявить функцию int binex_tx_callback(uint8_t c), посредством которой библиотека будет отправлять очередной символ в интерфейс связи.