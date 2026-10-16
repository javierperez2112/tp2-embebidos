# Pruebas de SPI y MCP25625

`App.c` tiene, por ahora, tres pruebas para los drivers de SPI (`spi.c`) y del
controlador CAN externo MCP25625 de la PlacaCAN (`mcp25625.c`). Las pruebas van
de menos a más hardware. Conviene correrlas en orden: si una falla, la anterior
dice si el problema es el SPI o el MCP.

| `APP_TEST`              | Prueba                    | Hardware necesario                              |
|-------------------------|---------------------------|-------------------------------------------------|
| `TEST_SPI_LOOP` (0)     | Solo el SPI               | Un cable entre D11 y D12, sin la PlacaCAN       |
| `TEST_CAN_LOOPBACK` (1) | SPI + MCP25625 sin bus    | PlacaCAN conectada y alimentada                 |
| `TEST_CAN_BUS` (2)      | CAN real contra otro nodo | PlacaCAN + al menos otra estación en el bus     |

## Elegir la prueba

Al principio de `App.c`:

```c
#define APP_TEST            TEST_CAN_LOOPBACK
```

También se puede elegir sin tocar el código, con `-DAPP_TEST=0`, `1` o `2` en
los símbolos del compilador del proyecto.

## Conexionado FRDM-K64F ↔ PlacaCAN

Los pines están en `board.h`. Se usa el header Arduino de la FRDM.

| FRDM       | Señal     | PlacaCAN | Notas                                   |
|------------|-----------|----------|-----------------------------------------|
| D10, PTD0  | CS        | J4.7     | SPI0_PCS0, activo bajo                  |
| D13, PTD1  | SCK       | J4.4     | SPI0_SCK                                |
| D11, PTD2  | MOSI      | J4.5     | SPI0_SOUT, va al SI del MCP25625        |
| D12, PTD3  | MISO      | J4.6     | SPI0_SIN, viene del SO del MCP25625     |
| D2, PTB9   | INT       | J4.3     | Obligatorio: el driver funciona por INT |
| D4, PTB23  | RESET     | J4.8     | Opcional, la PlacaCAN tiene pull-up     |
| GND        | GND       | J4.1     |                                         |

**INT es obligatorio.** Si no está conectado, el driver nunca se entera de que
llegó un frame o de que se liberó un buffer de TX.

### Jumpers de la PlacaCAN

| Jumper | Posición          | Para qué                                                   |
|--------|-------------------|------------------------------------------------------------|
| J7, J8 | Puestos           | Une el controlador con el transceiver                      |
| J9     | VREGIO – VIO      | Lógica a 3,3 V desde el regulador de la PlacaCAN           |
| J5     | 5V – VCAN         | 5 V desde el bus (VCC_CAN = 5 V)                           |
| J1, J2 | Solo en un extremo| Terminador de 120 Ω, únicamente en los dos extremos del bus|

Sin bus, para la prueba de loopback en el banco, los 5 V pueden entrar por el
pin 4 (VCAN) del conector CAN1/CAN2 desde una fuente.

> **¡Peligro!** No unir VIO ni 5V de la PlacaCAN con la FRDM si la PlacaCAN ya
> los genera con sus jumpers: es un cortocircuito. Entre las dos placas solo van
> GND y las señales de la tabla.

## LEDs

| LED                       | Significado                                         |
|---------------------------|-----------------------------------------------------|
| Rojo fijo desde el inicio | Falló la inicialización, ver [Problemas](#problemas)|
| Verde titilando           | Cada prueba que sale bien lo invierte               |
| Rojo (después de arrancar)| Hubo al menos un error, queda prendido              |
| Azul                      | Titila con cada frame recibido (solo `TEST_CAN_BUS`)|

En el debugger se pueden mirar las variables globales `testInitOk`, `testOk`,
`testErrors` y `testRx`.

## Las pruebas

### 0. `TEST_SPI_LOOP`: solo el SPI

1. Desconectar la PlacaCAN.
2. Unir D11 (MOSI, PTD2) con D12 (MISO, PTD3) con un cable.
3. Compilar con `APP_TEST` en `TEST_SPI_LOOP` y correr.

Al arrancar hace una transferencia bloqueante de 2 bytes. Si no vuelven iguales
(falta el puente, por ejemplo), queda en rojo fijo. Después, cada 100 ms, manda
16 bytes por interrupciones y compara lo recibido con lo enviado.

**Esperado:** verde titilando a 5 Hz, `testErrors` en 0.

Con el osciloscopio en D13 (SCK) y D10 (CS) se ve el CS bajo durante los 16
bytes seguidos, y SCK a unos 1,8 MHz.

### 1. `TEST_CAN_LOOPBACK`: SPI + MCP25625

1. Sacar el puente de la prueba anterior.
2. Conectar la PlacaCAN según la tabla y alimentarla.
3. Compilar con `APP_TEST` en `TEST_CAN_LOOPBACK` y correr.

El MCP25625 queda en modo loopback: lo que se transmite vuelve por RX sin salir
al bus, así que no hace falta otra placa ni terminador. Pasa por los filtros de
aceptación igual que un frame real. Cada 100 ms manda un frame con el ID propio
(`0x100 + GROUP_NUMBER`) y largo que va rotando de 0 a 8 bytes, y espera
recibirlo idéntico en menos de 50 ms.

**Esperado:** verde titilando a 5 Hz, `testRx` subiendo, `testErrors` en 0.
El LED amarillo INT de la PlacaCAN destella con cada frame.

Esta prueba ya ejercita todo el camino: inicialización bloqueante, carga de
buffers de TX, interrupción por INT, lectura de buffers de RX y las colas.

### 2. `TEST_CAN_BUS`: contra otra estación

1. Conectar la PlacaCAN al bus CAN con al menos otra estación.
2. Poner J1 y J2 solo si la placa está en un extremo del bus.
3. Compilar con `APP_TEST` en `TEST_CAN_BUS` y correr.

Cada 500 ms manda un frame `R000`, `R001`, `R002`, ... con el ID propio, en el
formato de ángulos del enunciado. El LED azul titila con cada frame recibido de
los ID 0x100 a 0x107.

**Esperado:** verde titilando a 1 Hz y azul titilando cuando las otras
estaciones transmiten. El LED TX de la PlacaCAN destella al enviar y el RX al
recibir.

Para probar con dos placas del grupo, cargar la misma prueba en las dos con
`GROUP_NUMBER` distinto en `station.h`.

## Problemas

**Rojo fijo al arrancar** (`testInitOk` en false). El MCP25625 no respondió o
no se pudo leer lo que se le escribió. Revisar:

- Alimentación: los LEDs rojos VIO y 5V de la PlacaCAN tienen que estar prendidos.
- MOSI y MISO invertidos (D11 va a J4.5, D12 a J4.6).
- CS (D10 a J4.7) y GND en común.
- Con un solo cable suelto en MISO, el pull-up hace que se lea 0xFF y falla la
  verificación: es lo esperado.

**`TEST_CAN_BUS` en rojo con una sola placa.** Sin otro nodo nadie da ACK: el
MCP25625 reintenta el frame para siempre, los buffers de TX no se liberan, se
llena la cola de software y `mcpSend` empieza a fallar. No es un error del
driver: hace falta otra estación en el bus.

**Loopback anda pero el bus no.** El problema está del lado del transceiver o
del bus: J7 y J8 puestos, 5 V presentes, terminadores en los dos extremos (60 Ω
medidos entre CANH y CANL con todo apagado) y el mismo bitrate en todos los nodos.

## Parámetros

| Dónde          | Qué                 | Valor                                             |
|----------------|---------------------|---------------------------------------------------|
| `station.h`    | `GROUP_NUMBER`      | 2, el ID propio es `0x100 + GROUP_NUMBER`         |
| `station.h`    | `CAN_BITRATE`       | 125 kbit/s, pedido por el enunciado               |
| `mcp25625.c`   | `MCP_SPI_BAUDRATE`  | 2 MHz pedidos, 1,8 MHz reales (máx. 10 MHz del MCP)|
| `mcp25625.h`   | Colas TX / RX       | 16 / 32 frames                                    |
| `board.h`      | `TEST_PIN`          | PTC10, en alto durante las ISR                    |

## Uso de CPU

`TEST_PIN` (PTC10) queda en alto mientras corre cualquier ISR, incluidas las de
SPI0 y la de INT (puerto B). El ciclo de trabajo de esa señal en el
osciloscopio es el porcentaje de CPU que se va en interrupciones. En
`TEST_CAN_LOOPBACK` cada frame son unas pocas transferencias SPI cortas: se ven
como ráfagas breves de pulsos cada 100 ms.
