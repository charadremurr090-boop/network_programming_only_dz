# network_programming_only_dz

# hw02 (13.11)

<img width="1428" height="824" alt="image" src="https://github.com/user-attachments/assets/227273bd-3c92-451a-9d9c-8642ffeb377f" />

# hw03 (13.11)

Подключаем сервер и клиент:

<img width="632" height="146" alt="image" src="https://github.com/user-attachments/assets/dac0f65e-5af5-4360-b9f0-8402db8cfd96" />

<img width="431" height="203" alt="image" src="https://github.com/user-attachments/assets/6dc39502-d149-4b64-93fc-203863a135cc" />

Программа не падает, если сервер отключён, а выводит ожидаемую ошибку:

<img width="545" height="61" alt="image" src="https://github.com/user-attachments/assets/ad7ffa49-143d-4621-9fca-6a0fa7d2144f" />

Схема:

```mermaid
sequenceDiagram
    participant C as Клиент (calc_client)
    participant S as Сервер (calc_server, порт 9000)
    C->>S: SYN (seq = x)
    S-->>C: SYN-ACK (seq = y, ack = x+1)
    C->>S: ACK (ack = y+1)
    Note over C: connect() вернул 0
    C->>S: данные "2+3*4\n"
    S-->>C: данные "OK 14\n"
```
# hw04 (17.11)

Запуск сервера и трёх клиентов + проверка эхо и подключения/отключения:

<img width="1438" height="816" alt="image" src="https://github.com/user-attachments/assets/966fab05-9778-4136-a392-e25b9082f670" />

(Эссе представлено в папке hw04)

hw03 (17.11)

запуск select_server и wepoll:

<img width="1035" height="58" alt="image" src="https://github.com/user-attachments/assets/fee3fd45-5fce-4700-bd51-8b6594ae0d2f" />

клиент получает эхо:

<img width="330" height="79" alt="image" src="https://github.com/user-attachments/assets/a6be38c7-e877-4596-8e14-aee0679c4d62" />


Таблица:

| Механизм | Сложность | Предел дескрипторов | Где доступно | Когда выбирать |
|---|---|---|---|---|
| select | O(n) | FD_SETSIZE (в Windows 64) | Везде (Windows / Linux / macOS) | Простые задачи, до 60-100 соединений. |
| poll | O(n) | Без жёсткого лимита (ограничено памятью) | POSIX (Linux / macOS) | Переносимый код без лимита select, если производительность не критична. |
| epoll | O(готовых) | Память ядра (тысячи и миллионы) | Linux (на Windows через wepoll) | Высоконагруженные серверы с тысячами соединений. |
| kqueue | O(готовых) | Память ядра (тысячи и миллионы) | BSD, macOS | Аналог epoll для систем BSD/macOS. |
