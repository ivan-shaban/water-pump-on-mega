### TODOs

- отправлять среднее время работы насоса в mqtt
- добавить проверку что если работа насоса превышает среднее время работы насоса, то сбрасывать его и слать лог с ошибкой
- почитать про перегрузку функций в С
- добавить очередь команды которые будут посылаться в mqtt
- добавить поддержку датчика протечки и в случае срабатывания прерывать работу насоса, также слать сообщение с ошибкой в mqtt

- проверить кейс потери соединения с wifi \ mqtt, кажется это блочит основной поток и приводит к переливанию жидкости
