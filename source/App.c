/***************************************************************************//**
  @file     App.c
  @brief    Control de acceso a un edificio de oficinas (TP1 - Interrupciones).

            Capa de aplicacion pura: no toca un solo registro ni un solo pin
            del hardware del equipo. Habla unicamente con los drivers:

              display.h  -> que texto/puntos/LEDs de la placa mostrar
              encoder.h  -> cuanto giro y si hubo click
              magcard.h  -> estado del lector y datos de la ultima tarjeta
              pisr.h     -> millis(), unica base de tiempo de la app
              gpio.h     -> solo para el LED RGB de la FRDM (pestillo/estado)

            No hay codigo bloqueante: App_Run() hace UNA pasada y vuelve
            (main.c ya la llama dentro de __FOREVER__). Todos los tiempos se
            miden por diferencia contra millis(), nunca con delays. Ningun
            evento de usuario se pierde: el encoder se polea a 1kHz puertas
            adentro del driver y acumula cuentas y clicks, y el lector arma la
            trama entera por interrupciones.

 *------------------------------------------------------------------------------
 * MANUAL DE USUARIO
 *------------------------------------------------------------------------------
 *
 * El equipo se maneja con UN solo control: el encoder. Girar elige, apretar
 * confirma. Como el driver del encoder entrega solo giro y click corto (no hay
 * pulsacion larga), las acciones que en otro equipo serian "otro boton" viven
 * dentro del propio alfabeto de edicion: girando mas alla del 9 aparecen las
 * opciones especiales.
 *
 *   Alfabeto al editar un numero:  0 1 2 3 4 5 6 7 8 9 - C [E]
 *
 *       0..9  digito a cargar
 *       '-'   borrar el ultimo digito (si no hay ninguno, cancela)
 *       'C'   cancelar el intento y volver
 *       'E'   aceptar (solo en el PIN, cuando ya tiene 4 digitos)
 *
 * El digito que se esta editando se marca con el PUNTO DECIMAL parpadeando,
 * asi el valor elegido queda siempre legible (a diferencia de hacer parpadear
 * el digito entero). Como el display tiene 4 caracteres y el ID tiene 8, la
 * ventana se desplaza sola para seguir al cursor.
 *
 * Flujo normal:
 *
 *   Reposo ("HOLA" y despues una animacion)
 *     - girar  -> ajuste de intensidad del display ("br N"), click para salir
 *     - click  -> ingreso de ID (8 digitos)
 *     - pasar la tarjeta -> carga el ID solo y salta al PIN
 *   ID -> PIN (4 o 5 digitos, ocultos con guiones una vez cargados)
 *   Si ID y PIN son correctos: "OPEn", LED verde y pestillo liberado 5 s.
 *   Al cerrarse el pestillo queda abierto un menu de sesion:
 *       usuario:  PASS (cambiar clave)  /  SALI
 *       admin:    Add / dEL / UnbL / PASS / SALI
 *
 * Seguridad:
 *   - PIN incorrecto: penalidad creciente (2 s, 4 s) y a los 3 intentos el ID
 *     queda BLOQUEADO ("bLOC") hasta que un administrador lo desbloquee.
 *   - Un ID inexistente y un PIN incorrecto dan exactamente la misma respuesta
 *     ("Err"), para que probando no se pueda averiguar que IDs existen.
 *   - Toda pantalla que no sea el reposo vuelve sola al reposo tras 15 s sin
 *     actividad, borrando el PIN a medio tipear y cerrando la sesion.
 *   - La penalidad no se puede saltear con un click.
 *
 * Usuarios de fabrica:  admin 00000000 / 1234      usuario 12345678 / 4321
 *
 ******************************************************************************/

/*******************************************************************************
 * CONFIGURACION DE COMPILACION
 ******************************************************************************/

// Modo de puesta en marcha del hardware. En 0 (normal) corre la aplicacion y
// esta capa ni se entera de que existe una cadena de shift registers. En 1 la
// aplicacion entera se reemplaza por la rutina de PRUEBA DE HARDWARE de mas
// abajo, para cuando el display no muestra nada y hay que averiguar donde se
// corta la cadena.
#define APP_HW_TEST     0

/*******************************************************************************
 * INCLUDE HEADER FILES
 ******************************************************************************/

#include "board.h"
#include "gpio.h"
#include "pisr.h"
#include "display.h"
#include "encoder.h"
#include "magcard.h"
#include "strutil.h"

#if APP_HW_TEST
#include "shift_register.h"     // solo la prueba de hardware habla con la cadena
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*******************************************************************************
 * CONSTANT AND MACRO DEFINITIONS USING #DEFINE
 ******************************************************************************/

// Cuentas del encoder por cada paso mecanico (detent)
#define ENCODER_COUNTS_PER_DETENT   4

// Largos de las credenciales (los fija la consigna)
#define ID_LEN          8
#define PIN_MIN_LEN     4
#define PIN_MAX_LEN     5

#define MAX_USERS       8
#define MAX_FAILS       3       // intentos fallidos antes de bloquear el ID

// Tiempos (ms). Ninguno se espera bloqueando: son diferencias contra millis().
#define DOOR_OPEN_MS        5000u   // pestillo liberado, lo fija la consigna
#define INACTIVITY_MS      15000u   // vuelta automatica al reposo
#define MSG_MS              1200u   // duracion de un mensaje informativo
#define PENALTY_STEP_MS     2000u   // castigo = PENALTY_STEP_MS * intento
#define BLINK_MS             350u   // semiperiodo del cursor
#define IDLE_ANIM_MS         150u   // paso de la animacion de reposo
#define HELLO_MS            1200u   // saludo al entrar en reposo
#define BOOT_TEST_MS        1200u   // prueba de display al encender

// Niveles de intensidad del display. El rango lo define el driver, que es
// quien sabe en cuantos escalones puede repartir el slot de cada digito.
#define BRIGHT_MIN      DISPLAY_BRIGHT_MIN
#define BRIGHT_MAX      DISPLAY_BRIGHT_MAX
#define BRIGHT_DEFAULT  BRIGHT_MAX

// Opciones del editor de numeros: 0..9 y despues las acciones
#define OPT_DIGITS      10
#define OPT_DEL         10      // '-'
#define OPT_CANCEL      11      // 'C'
#define OPT_ACCEPT      12      // 'E' (solo en el PIN, con largo suficiente)

// Valores de setLEDStatus(): los 3 LEDs de la placa son excluyentes entre si
// porque comparten el select de la cadena de shift registers.
#define LEDS_OFF        0
#define LEDS_ID         1       // pidiendo ID
#define LEDS_PIN        2       // pidiendo PIN
#define LEDS_OPEN       3       // pestillo liberado / sesion abierta

/*******************************************************************************
 * ENUMERATIONS AND STRUCTURES AND TYPEDEFS
 ******************************************************************************/

typedef enum {
    ST_BOOT = 0,    // prueba de display al encender: 8888 + puntos + LEDs
    ST_IDLE,        // reposo: saludo + animacion
    ST_BRIGHT,      // ajuste de intensidad del display
    ST_EDIT_ID,     // editor de ID (8 digitos, visibles)
    ST_EDIT_PIN,    // editor de PIN (4 o 5 digitos, ocultos)
    ST_MENU,        // menu de sesion (usuario o administrador)
    ST_MSG,         // mensaje transitorio, con estado siguiente
    ST_DOOR         // pestillo liberado
} app_state_t;

// Para que se esta pidiendo el numero que se esta editando. Un mismo editor
// sirve para todos los flujos; lo unico que cambia es que hacer al aceptar.
typedef enum {
    P_LOGIN_ID = 0, // ID para intentar entrar
    P_LOGIN_PIN,    // PIN de ese intento
    P_ADD_ID,       // ID del usuario nuevo (admin)
    P_ADD_PIN,      // PIN del usuario nuevo (admin)
    P_DEL_ID,       // ID a eliminar (admin)
    P_UNBLOCK_ID,   // ID a desbloquear (admin)
    P_CHANGE_PIN,   // clave nueva del usuario logueado
    P_CONFIRM_PIN   // repeticion de la clave nueva
} purpose_t;

typedef enum {
    MENU_USER = 0,
    MENU_ADMIN
} menu_id_t;

typedef struct {
    bool    used;
    bool    isAdmin;
    bool    blocked;
    uint8_t failCount;
    char    id[ID_LEN + 1];
    char    pin[PIN_MAX_LEN + 1];
} user_t;

/*******************************************************************************
 * VARIABLES WITH FILE SCOPE
 ******************************************************************************/

// ---- Base de usuarios -------------------------------------------------------
static user_t users[MAX_USERS] = {
    { true, true,  false, 0, "00000000", "1234" },   // administrador
    { true, false, false, 0, "12345678", "4321" }    // usuario de ejemplo
};

// ---- Maquina de estados -----------------------------------------------------
static app_state_t state          = ST_BOOT;
static uint32_t    stateTs        = 0;    // millis() al entrar al estado
static uint32_t    lastActivityTs = 0;    // ultimo evento de usuario

// ---- Editor de numeros (ID / PIN) -------------------------------------------
static char        editBuf[ID_LEN + 1];   // el ID es el mas largo de los dos
static uint8_t     editLen    = 0;        // digitos confirmados == posicion del cursor
static uint8_t     editMin    = 0;        // largo minimo aceptable
static uint8_t     editMax    = 0;        // largo maximo (al llegar, acepta solo)
static bool        editHide   = false;    // true: lo ya cargado se ve como '-'
static uint8_t     optIdx     = 0;        // opcion que se esta girando
static purpose_t   editPurpose     = P_LOGIN_ID;
static app_state_t editCancelState = ST_IDLE;   // a donde volver si cancela

// ---- Mensajes transitorios --------------------------------------------------
static char        msgBuf[MAX_CHARS];
static uint32_t    msgMs        = 0;
static app_state_t msgNext      = ST_IDLE;
static bool        msgSkippable = true;   // false para las penalidades
static bool        msgIsError   = false;  // define el color del LED RGB

// ---- Menus ------------------------------------------------------------------
static const char* const userMenu[]  = { "PASS", "SALI" };
static const char* const adminMenu[] = { "Add", "dEL", "UnbL", "PASS", "SALI" };

static menu_id_t menuId  = MENU_USER;
static uint8_t   menuIdx = 0;

// ---- Sesion y datos en transito ---------------------------------------------
static user_t* session = NULL;              // usuario logueado (NULL si no hay)
static char    pendingId[ID_LEN + 1];       // ID cargado, esperando el PIN
static char    pendingPin[PIN_MAX_LEN + 1]; // clave nueva, esperando confirmacion

// ---- Varios -----------------------------------------------------------------
static uint8_t brightness   = BRIGHT_DEFAULT;
static int32_t lastRawCount = 0;            // para convertir cuentas en detents
static char    lastText[MAX_CHARS];         // ultimo texto escrito al display
static bool    lastDots[MAX_CHARS];
static uint8_t lastLeds     = 0xFF;         // centinela: fuerza el primer seteo
static uint8_t lastRgb      = 0xFF;         // idem, para el LED RGB de la FRDM

/*******************************************************************************
 * FUNCTION PROTOTYPES WITH FILE SCOPE
 ******************************************************************************/

static int32_t  takeDetents (void);
static uint32_t elapsedSince (uint32_t ts);
static bool     blinkOn (void);
static uint8_t  wrapIndex (int32_t value, int32_t count);
static void     copyText (char dst[MAX_CHARS], const char* src);

static void handleInput (int32_t detents, bool click);
static void handleCard (void);
static void handleTimeouts (void);
static void render (void);
static void updateLeds (void);

static void gotoState (app_state_t next);
static void showMessage (const char* text, uint32_t ms, app_state_t next,
                         bool skippable, bool isError);

static void    startEdit (purpose_t purpose, app_state_t cancelState);
static void    editorRotate (int32_t detents);
static void    editorClick (void);
static void    editorAccept (void);
static void    editorCancel (void);
static void    editorRender (char buf[MAX_CHARS], bool dots[MAX_CHARS]);
static char    optionChar (uint8_t idx);
static uint8_t optionCount (void);

static void        openMenu (menu_id_t id);
static void        menuClick (void);
static uint8_t     menuCount (void);
static const char* menuLabel (void);

static void validateLogin (void);
static void openDoor (void);
static void closeDoor (void);
static void logout (void);
static void addUser (void);
static void deleteUser (void);
static void unblockUser (void);
static void changePin (void);

static user_t* findUser (const char* id);
static bool    idFromPan (const char* pan, char out[ID_LEN + 1]);
static void    wipeSecrets (void);

static void adjustBrightness (int32_t detents);
static void applyBrightness (void);

static void ledWrite (pin_t pin, bool on);
static void ledRgb (bool red, bool green, bool blue);

/*******************************************************************************
 *******************************************************************************
                            PRUEBA DE HARDWARE
 *******************************************************************************
 ******************************************************************************/

/*
 * Rutina de puesta en marcha, para usar cuando el display no muestra nada.
 * Se activa poniendo APP_HW_TEST en 1 (arriba de todo) y reemplaza a la
 * aplicacion entera: no multiplexa, no usa el driver de display, no depende
 * del encoder ni del lector, y no hay ninguna ISR compitiendo por la cadena.
 * Solo empuja tramas fijas y las deja quietas un segundo, que es tiempo de
 * sobra para medirlas con un tester.
 *
 * El ciclo dura 20 s y tiene dos fases:
 *
 *   FASE A - 4 s, LED azul de la FRDM encendido.
 *     Prende un digito entero por vez (los 7 segmentos mas el punto), un
 *     segundo cada uno. Contesta la primera pregunta: se enciende algo?
 *       - Si en los 4 segundos no se enciende NADA, el corte esta antes del
 *         display: alimentacion del protoboard, masa comun con la FRDM, o la
 *         cadena de 595 no esta recibiendo reloj/latch.
 *       - Si se encienden segmentos pero en el digito equivocado, el par de
 *         bits de seleccion no cae donde el codigo cree.
 *
 *   FASE B - 16 s, LED verde de la FRDM encendido durante la etapa 0.
 *     Camina un unico 1 por las 16 etapas de la cadena, un segundo por etapa
 *     y todo lo demas en cero. La etapa 0 es QA del 595 mas cercano al micro
 *     y la 15 es QH del mas lejano. Anotando que se prende en cada paso queda
 *     levantado el mapa real de la cadena, que es justo lo que
 *     pushDigitFrame() (display.c) da por supuesto:
 *
 *         etapa 0       punto decimal
 *         etapas 1..7   segmentos g, f, e, d, c, b, a
 *         etapas 8..9   seleccion de digito (bit 0, bit 1)
 *         etapas 10..11 LEDs de estado (bit 0, bit 1)
 *         etapas 12..15 sin conectar
 *
 *     Si el orden real resulta ser otro, el arreglo es reordenar los push de
 *     pushDigitFrame(); ni la aplicacion ni el driver de shift register se
 *     enteran del cambio.
 */

#if APP_HW_TEST

#define HWTEST_CHAIN_BITS       16u     // dos 74HC595 en cascada
#define HWTEST_STEP_MS        1000u     // un paso por segundo, medible a mano
#define HWTEST_DIGIT_STEPS       4u     // fase A: un paso por digito
#define HWTEST_STEPS            (HWTEST_DIGIT_STEPS + HWTEST_CHAIN_BITS)

/*
 * @brief Deja la cadena mostrando el paso pedido y no la vuelve a tocar.
 */
static void hwTestShow (uint8_t step)
{
    if (step < HWTEST_DIGIT_STEPS)
    {
        // FASE A: un digito entero, quieto. Mismo orden de bits que usa
        // pushDigitFrame(), asi la prueba valida la trama real y no otra.
        SR_PushBits(0u, 2u);        // LEDs de estado apagados
        SR_PushBits(step, 2u);      // seleccion de digito
        SR_PushBits(0xFFu, 8u);     // 7 segmentos + punto, todo encendido
        SR_Latch();

        ledWrite(PIN_LED_BLUE, true);
        ledWrite(PIN_LED_GREEN, false);
    }
    else
    {
        // FASE B: un solo 1 recorriendo la cadena, de QA del primer 595 en
        // adelante. SR_Write() empuja el bit mas significativo primero, que es
        // el que viaja mas lejos, asi que 1<<pos cae en la etapa pos.
        uint8_t pos = (uint8_t)(step - HWTEST_DIGIT_STEPS);

        SR_Write(1UL << pos, HWTEST_CHAIN_BITS);

        ledWrite(PIN_LED_BLUE, false);
        ledWrite(PIN_LED_GREEN, pos == 0u);     // marca para poder contar
    }
}

static void hwTestInit (void)
{
    gpioMode(PIN_LED_RED, OUTPUT);
    gpioMode(PIN_LED_GREEN, OUTPUT);
    gpioMode(PIN_LED_BLUE, OUTPUT);
    ledWrite(PIN_LED_RED, false);
    ledWrite(PIN_LED_GREEN, false);
    ledWrite(PIN_LED_BLUE, false);

    pisrInit();                 // solo para tener millis(): nadie registra nada
    SR_Init();
    SR_OutputEnable(true);      // la cadena queda habilitada todo el tiempo

    hwTestShow(0u);
}

static void hwTestRun (void)
{
    static uint32_t stepTs = 0u;
    static uint8_t  step   = 0u;

    if ((millis() - stepTs) < HWTEST_STEP_MS)
    {
        return;
    }

    stepTs = millis();
    step   = (uint8_t)((step + 1u) % HWTEST_STEPS);

    hwTestShow(step);
}

#endif // APP_HW_TEST

/*******************************************************************************
 *******************************************************************************
                        GLOBAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/*
 * @brief Se llama una sola vez, con las interrupciones deshabilitadas.
 */
void App_Init (void)
{
#if APP_HW_TEST
    hwTestInit();       // la aplicacion no arranca: ver PRUEBA DE HARDWARE
    return;
#endif

    // El LED RGB de la FRDM es lo unico que la app maneja por GPIO: emula el
    // pestillo electrico (verde) e informa el estado (azul/rojo).
    gpioMode(PIN_LED_RED, OUTPUT);
    gpioMode(PIN_LED_GREEN, OUTPUT);
    gpioMode(PIN_LED_BLUE, OUTPUT);
    ledRgb(false, false, false);    // sin esto arrancan prendidos (activos bajos)

    pisrInit();         // base de tiempo (millis) antes que cualquier driver
    displayInit();      // multiplexa sus 4 digitos solo, via pisr
    Encoder_Init();     // se polea solo a 1 kHz, via pisr
    MagCard_Init();     // trabaja por interrupciones de GPIO

    applyBrightness();

    lastRawCount   = Encoder_GetCount();
    stateTs        = millis();
    lastActivityTs = stateTs;
}

/*
 * @brief Una pasada del lazo principal. main.c la llama dentro de __FOREVER__,
 * asi que esta funcion nunca debe bloquear ni tener su propio while(1).
 */
void App_Run (void)
{
#if APP_HW_TEST
    hwTestRun();        // la aplicacion no corre: ver PRUEBA DE HARDWARE
    return;
#endif

    int32_t detents = takeDetents();
    bool    click   = Encoder_GetClick();

    if ((detents != 0) || click)
    {
        lastActivityTs = millis();
    }

    handleInput(detents, click);
    handleCard();
    handleTimeouts();
    render();
}

/*******************************************************************************
 *******************************************************************************
                        LOCAL FUNCTION DEFINITIONS
 *******************************************************************************
 ******************************************************************************/

/*------------------------------------------------------------------------------
 * Lazo principal: entradas, tarjeta y tiempos
 *----------------------------------------------------------------------------*/

/*
 * @brief Reparte el giro y el click segun el estado actual.
 */
static void handleInput (int32_t detents, bool click)
{
    switch (state)
    {
        case ST_IDLE:
            // En reposo el encoder regula la intensidad del display; el click
            // arranca un intento de acceso.
            if (detents != 0)
            {
                gotoState(ST_BRIGHT);
                adjustBrightness(detents);
            }
            else if (click)
            {
                startEdit(P_LOGIN_ID, ST_IDLE);
            }
            break;

        case ST_BRIGHT:
            if (detents != 0)
            {
                adjustBrightness(detents);
            }
            if (click)
            {
                gotoState(ST_IDLE);
            }
            break;

        case ST_EDIT_ID:
        case ST_EDIT_PIN:
            if (detents != 0)
            {
                editorRotate(detents);
            }
            if (click)
            {
                editorClick();
            }
            break;

        case ST_MENU:
            if (detents != 0)
            {
                menuIdx = wrapIndex((int32_t)menuIdx + detents, (int32_t)menuCount());
            }
            if (click)
            {
                menuClick();
            }
            break;

        case ST_MSG:
            // Los mensajes informativos se pueden saltear con un click, pero
            // las penalidades por PIN incorrecto no: son parte de la defensa
            // contra la prueba de claves por fuerza bruta.
            if (click && msgSkippable)
            {
                gotoState(msgNext);
            }
            break;

        case ST_DOOR:
            break;  // el pestillo dura sus 5 s completos

        default:
            break;
    }
}

/*
 * @brief Atiende al lector de tarjetas. La tarjeta es simplemente una forma
 * rapida de llenar el editor de ID: se carga y se acepta como si el usuario lo
 * hubiera tipeado, asi sirve para entrar y tambien para las altas y bajas del
 * modo administrador.
 */
static void handleCard (void)
{
    magcard_status_t st = MagCard_GetStatus();

    if ((st == MAGCARD_IDLE) || (st == MAGCARD_READING))
    {
        return;     // no hay nada resuelto todavia
    }

    // La tarjeta solo significa algo cuando el equipo esta esperando un ID. En
    // cualquier otro estado (PIN, pestillo abierto, menu, mensaje) el swipe se
    // descarta: avisar ahi tendria que interrumpir lo que se esta mostrando, y
    // volver "al estado actual" cuando el estado actual ya es un mensaje o el
    // pestillo abierto reiniciaria su propio temporizador.
    if ((state == ST_IDLE) || (state == ST_EDIT_ID))
    {
        if (st == MAGCARD_OK)
        {
            char pan[MAGCARD_MAX_PAN_LEN];
            char extra[MAGCARD_MAX_DATA_LEN];
            char id[ID_LEN + 1];

            MagCard_GetTrack2(pan, sizeof(pan), extra, sizeof(extra));

            if (!idFromPan(pan, id))
            {
                showMessage("nOId", MSG_MS, state, true, true);
            }
            else
            {
                lastActivityTs = millis();

                if (state == ST_IDLE)
                {
                    startEdit(P_LOGIN_ID, ST_IDLE);
                }

                memCopy(editBuf, id, ID_LEN);
                editLen = ID_LEN;
                editorAccept();
            }
        }
        else
        {
            // Errores del lector: se distinguen para poder diagnosticar el
            // swipe (paridad, LRC, trama incompleta, desborde).
            const char* text = "ErrC";

            switch (st)
            {
                case MAGCARD_ERROR_PARITY:   text = "PArI"; break;
                case MAGCARD_ERROR_LRC:      text = "LrC";  break;
                case MAGCARD_ERROR_FORMAT:   text = "FOr";  break;
                case MAGCARD_ERROR_OVERFLOW: text = "OFLO"; break;
                default:                                    break;
            }

            showMessage(text, MSG_MS, state, true, true);
        }
    }

    MagCard_Clear();
}

/*
 * @brief Vencimientos: mensajes, pestillo e inactividad.
 */
static void handleTimeouts (void)
{
    switch (state)
    {
        case ST_BOOT:
            if (elapsedSince(stateTs) >= BOOT_TEST_MS)
            {
                gotoState(ST_IDLE);
            }
            break;

        case ST_MSG:
            if (elapsedSince(stateTs) >= msgMs)
            {
                gotoState(msgNext);
            }
            break;

        case ST_DOOR:
            if (elapsedSince(stateTs) >= DOOR_OPEN_MS)
            {
                closeDoor();
            }
            break;

        case ST_IDLE:
            break;      // el reposo no vence

        default:
            // Cualquier pantalla intermedia vuelve sola al reposo, borrando lo
            // que se hubiera tipeado y cerrando la sesion abierta.
            if (elapsedSince(lastActivityTs) >= INACTIVITY_MS)
            {
                logout();
            }
            break;
    }
}

/*------------------------------------------------------------------------------
 * Salida: display y LEDs
 *----------------------------------------------------------------------------*/

/*
 * @brief Arma los 4 caracteres y los 4 puntos decimales que corresponden al
 * estado actual y los manda al display solo si cambiaron respecto de la pasada
 * anterior (evita reescribir a cada vuelta el buffer que lee el multiplexado).
 */
static void render (void)
{
    char    buf[MAX_CHARS];
    bool    dots[MAX_CHARS] = { false, false, false, false };
    uint8_t i;

    switch (state)
    {
        case ST_BOOT:
            // Prueba de encendido: enciende TODOS los segmentos de los cuatro
            // dígitos, los cuatro puntos y los LEDs. Si al energizar el equipo
            // no aparece un 8888 completo durante un segundo, el problema no
            // está en la aplicación sino más abajo (driver, cadena de shift
            // registers o alimentación de la placa).
            copyText(buf, "8888");
            for (i = 0; i < MAX_CHARS; i++)
            {
                dots[i] = true;
            }
            break;

        case ST_IDLE:
            if (elapsedSince(stateTs) < HELLO_MS)
            {
                copyText(buf, "HOLA");
            }
            else
            {
                // Barra de guiones que crece de izquierda a derecha y después
                // se retira. Siempre quedan uno a cuatro segmentos prendidos:
                // un único segmento moviéndose, multiplexado al 25%, es
                // demasiado tenue para verse a un metro del equipo.
                uint8_t frame = (uint8_t)((millis() / IDLE_ANIM_MS) % 7u);

                for (i = 0; i < MAX_CHARS; i++)
                {
                    bool on = (frame < MAX_CHARS) ? (i <= frame)
                                                  : (i > (frame - MAX_CHARS));
                    buf[i] = on ? '-' : ' ';
                }
            }
            break;

        case ST_BRIGHT:
            copyText(buf, "br");
            buf[MAX_CHARS - 1] = (char)('0' + brightness);
            break;

        case ST_EDIT_ID:
        case ST_EDIT_PIN:
            editorRender(buf, dots);
            break;

        case ST_MENU:
            copyText(buf, menuLabel());
            break;

        case ST_MSG:
            memCopy(buf, msgBuf, MAX_CHARS);
            break;

        case ST_DOOR:
            copyText(buf, "OPEn");
            break;

        default:
            copyText(buf, "");
            break;
    }

    if (memCompare(buf, lastText, MAX_CHARS) != 0)
    {
        displayWriteString(buf);
        memCopy(lastText, buf, MAX_CHARS);
    }

    for (i = 0; i < MAX_CHARS; i++)
    {
        if (dots[i] != lastDots[i])
        {
            setDecimalPoint(i, dots[i]);
            lastDots[i] = dots[i];
        }
    }

    updateLeds();
}

/*
 * @brief Politica de LEDs, toda en un solo lugar. Los 3 de la placa (via shift
 * register) indican la etapa; el RGB de la FRDM hace de pestillo (verde) y de
 * semaforo (azul trabajando, rojo rechazo).
 */
static void updateLeds (void)
{
    uint8_t leds  = LEDS_OFF;
    bool    red   = false;
    bool    green = false;
    bool    blue  = false;

    switch (state)
    {
        case ST_BOOT:
            // Todo encendido a la vez, para poder verificarlo de un vistazo.
            leds  = LEDS_ID;
            red   = true;
            green = true;
            blue  = true;
            break;

        case ST_EDIT_ID:
            leds = LEDS_ID;
            blue = true;
            break;

        case ST_EDIT_PIN:
            leds = LEDS_PIN;
            blue = true;
            break;

        case ST_DOOR:
            leds  = LEDS_OPEN;
            green = true;               // pestillo liberado
            break;

        case ST_MENU:
            leds  = LEDS_OPEN;
            green = blinkOn();          // sesion abierta: verde parpadeando
            break;

        case ST_MSG:
            red   = msgIsError;
            green = !msgIsError;
            break;

        default:
            break;
    }

    if (leds != lastLeds)
    {
        setLEDStatus(leds);
        lastLeds = leds;
    }

    ledRgb(red, green, blue);
}

/*------------------------------------------------------------------------------
 * Transiciones de estado
 *----------------------------------------------------------------------------*/

static void gotoState (app_state_t next)
{
    state          = next;
    stateTs        = millis();
    lastActivityTs = stateTs;   // un estado recien entrado no vence de una
}

/*
 * @brief Muestra un texto durante ms y despues pasa a next.
 * @params skippable Si el click puede acortar la espera (false en penalidades)
 * @params isError   Colorea el LED RGB (rojo) y distingue aviso de exito
 */
static void showMessage (const char* text, uint32_t ms, app_state_t next,
                         bool skippable, bool isError)
{
    copyText(msgBuf, text);
    msgMs        = ms;
    msgNext      = next;
    msgSkippable = skippable;
    msgIsError   = isError;
    gotoState(ST_MSG);
}

/*------------------------------------------------------------------------------
 * Editor de numeros (sirve para el ID y para el PIN)
 *----------------------------------------------------------------------------*/

/*
 * @brief Arranca la edicion de un numero. El proposito decide el largo, si se
 * oculta y que se hace al aceptarlo.
 */
static void startEdit (purpose_t purpose, app_state_t cancelState)
{
    bool isPin = (purpose != P_LOGIN_ID) && (purpose != P_ADD_ID) &&
                 (purpose != P_DEL_ID)   && (purpose != P_UNBLOCK_ID);

    editPurpose     = purpose;
    editCancelState = cancelState;
    editLen         = 0;
    optIdx          = 0;
    editHide        = isPin;
    editMin         = isPin ? PIN_MIN_LEN : ID_LEN;
    editMax         = isPin ? PIN_MAX_LEN : ID_LEN;

    memFill(editBuf, 0, sizeof(editBuf));

    gotoState(isPin ? ST_EDIT_PIN : ST_EDIT_ID);
}

/*
 * @brief El giro recorre el alfabeto de opciones, con vuelta redonda.
 */
static void editorRotate (int32_t detents)
{
    optIdx = wrapIndex((int32_t)optIdx + detents, (int32_t)optionCount());
}

/*
 * @brief El click confirma la opcion que se esta mostrando.
 */
static void editorClick (void)
{
    if (optIdx < OPT_DIGITS)
    {
        editBuf[editLen++] = (char)('0' + optIdx);
        optIdx = 0;

        if (editLen >= editMax)
        {
            editorAccept();     // al completar el largo maximo, acepta solo
        }
    }
    else if (optIdx == OPT_DEL)
    {
        if (editLen == 0)
        {
            editorCancel();     // borrar sin nada cargado equivale a cancelar
        }
        else
        {
            editLen--;
            // Se recupera el digito borrado como opcion actual, para poder
            // corregirlo girando poquito en vez de volver a buscarlo.
            optIdx = (uint8_t)(editBuf[editLen] - '0');
            editBuf[editLen] = '\0';
        }
    }
    else if (optIdx == OPT_CANCEL)
    {
        editorCancel();
    }
    else    /* OPT_ACCEPT */
    {
        editorAccept();
    }
}

/*
 * @brief Numero terminado: se ejecuta la accion que lo pidio.
 */
static void editorAccept (void)
{
    editBuf[editLen] = '\0';

    switch (editPurpose)
    {
        case P_LOGIN_ID:
        {
            memCopy(pendingId, editBuf, ID_LEN + 1);

            user_t* u = findUser(pendingId);
            if (u == NULL)
            {
                // El ID no existe: no tiene sentido pedir un PIN para nadie.
                wipeSecrets();
                showMessage("Err", PENALTY_STEP_MS, ST_IDLE, false, true);
            }
            else if (u->blocked)
            {
                // No tiene sentido pedir el PIN de un ID bloqueado: se avisa
                // de una vez, tanto si el ID vino tipeado como de la tarjeta.
                wipeSecrets();
                showMessage("bLOC", PENALTY_STEP_MS, ST_IDLE, false, true);
            }
            else
            {
                startEdit(P_LOGIN_PIN, ST_IDLE);
            }
            break;
        }

        case P_LOGIN_PIN:
            validateLogin();
            break;

        case P_ADD_ID:
            memCopy(pendingId, editBuf, ID_LEN + 1);
            startEdit(P_ADD_PIN, ST_MENU);
            break;

        case P_ADD_PIN:
            addUser();
            break;

        case P_DEL_ID:
            deleteUser();
            break;

        case P_UNBLOCK_ID:
            unblockUser();
            break;

        case P_CHANGE_PIN:
            // La clave nueva se pide dos veces: un error de tipeo aca dejaria
            // al usuario afuera de su propia oficina.
            memCopy(pendingPin, editBuf, PIN_MAX_LEN + 1);
            startEdit(P_CONFIRM_PIN, ST_MENU);
            break;

        case P_CONFIRM_PIN:
            changePin();
            break;

        default:
            logout();
            break;
    }
}

/*
 * @brief Intento cancelado: se borra todo rastro y se vuelve.
 */
static void editorCancel (void)
{
    memFill(editBuf, 0, sizeof(editBuf));
    editLen = 0;
    optIdx  = 0;

    if ((editCancelState == ST_MENU) && (session != NULL))
    {
        gotoState(ST_MENU);
    }
    else
    {
        logout();
    }
}

/*
 * @brief Dibuja la ventana de 4 caracteres del numero que se esta editando. La
 * ventana sigue al cursor, que se marca con el punto decimal parpadeando.
 */
static void editorRender (char buf[MAX_CHARS], bool dots[MAX_CHARS])
{
    uint8_t window = (editLen < MAX_CHARS) ? 0u
                                           : (uint8_t)(editLen - MAX_CHARS + 1u);
    uint8_t k;

    for (k = 0; k < MAX_CHARS; k++)
    {
        uint8_t idx = (uint8_t)(window + k);

        if (idx == editLen)
        {
            // Cursor: se ve la opcion que se esta girando, siempre legible, y
            // el punto decimal parpadea para marcar donde estamos parados.
            buf[k]  = optionChar(optIdx);
            dots[k] = blinkOn();
        }
        else if (idx < editLen)
        {
            // Los digitos del PIN ya cargados quedan ocultos (lo pide la
            // consigna); los del ID se muestran.
            buf[k] = editHide ? '-' : editBuf[idx];
        }
        else
        {
            buf[k] = ' ';
        }
    }
}

/*
 * @brief Caracter que representa a cada opcion del alfabeto de edicion.
 */
static char optionChar (uint8_t idx)
{
    if (idx < OPT_DIGITS)
    {
        return (char)('0' + idx);
    }
    if (idx == OPT_DEL)
    {
        return '-';
    }
    if (idx == OPT_CANCEL)
    {
        return 'C';
    }

    return 'E';     /* OPT_ACCEPT */
}

/*
 * @brief Cuantas opciones tiene el alfabeto ahora mismo. "Aceptar" solo
 * aparece en el PIN y recien cuando alcanzo el largo minimo.
 */
static uint8_t optionCount (void)
{
    uint8_t count = OPT_DIGITS + 2u;    // digitos + borrar + cancelar

    if ((state == ST_EDIT_PIN) && (editLen >= editMin))
    {
        count++;                        // + aceptar
    }

    return count;
}

/*------------------------------------------------------------------------------
 * Menus de sesion
 *----------------------------------------------------------------------------*/

static void openMenu (menu_id_t id)
{
    menuId  = id;
    menuIdx = 0;
    gotoState(ST_MENU);
}

static uint8_t menuCount (void)
{
    return (menuId == MENU_ADMIN)
                ? (uint8_t)(sizeof(adminMenu) / sizeof(adminMenu[0]))
                : (uint8_t)(sizeof(userMenu)  / sizeof(userMenu[0]));
}

static const char* menuLabel (void)
{
    return (menuId == MENU_ADMIN) ? adminMenu[menuIdx] : userMenu[menuIdx];
}

static void menuClick (void)
{
    if (menuId == MENU_USER)
    {
        switch (menuIdx)
        {
            case 0:  startEdit(P_CHANGE_PIN, ST_MENU); break;   // PASS
            default: logout();                         break;   // SALI
        }
    }
    else
    {
        switch (menuIdx)
        {
            case 0:  startEdit(P_ADD_ID, ST_MENU);     break;   // Add
            case 1:  startEdit(P_DEL_ID, ST_MENU);     break;   // dEL
            case 2:  startEdit(P_UNBLOCK_ID, ST_MENU); break;   // UnbL
            case 3:  startEdit(P_CHANGE_PIN, ST_MENU); break;   // PASS
            default: logout();                         break;   // SALI
        }
    }
}

/*------------------------------------------------------------------------------
 * Reglas de acceso
 *----------------------------------------------------------------------------*/

/*
 * @brief Valida el par ID/PIN recien ingresado y decide que pasa.
 */
static void validateLogin (void)
{
    user_t* u = findUser(pendingId);

    if ((u != NULL) && u->blocked)
    {
        // Al usuario bloqueado si se le avisa: necesita saber que tiene que ir
        // a buscar a un administrador.
        wipeSecrets();
        showMessage("bLOC", PENALTY_STEP_MS, ST_IDLE, false, true);
        return;
    }

    if ((u == NULL) || (strCompare(u->pin, editBuf) != 0))
    {
        // Un ID inexistente y un PIN equivocado se responden igual, para que
        // probando no se pueda averiguar que IDs estan dados de alta.
        uint32_t penalty = PENALTY_STEP_MS;

        if (u != NULL)
        {
            u->failCount++;
            penalty = PENALTY_STEP_MS * u->failCount;

            if (u->failCount >= MAX_FAILS)
            {
                u->blocked = true;
                wipeSecrets();
                showMessage("bLOC", penalty, ST_IDLE, false, true);
                return;
            }
        }

        wipeSecrets();
        showMessage("Err", penalty, ST_IDLE, false, true);
        return;
    }

    u->failCount = 0;
    session      = u;
    wipeSecrets();
    openDoor();
}

/*
 * @brief Libera el pestillo (emulado con el LED verde de la FRDM).
 */
static void openDoor (void)
{
    gotoState(ST_DOOR);
}

/*
 * @brief Se cumplieron los 5 s: se cierra el pestillo y queda abierto el menu
 * de la sesion (asi el usuario puede, por ejemplo, cambiar su clave).
 */
static void closeDoor (void)
{
    if (session != NULL)
    {
        openMenu(session->isAdmin ? MENU_ADMIN : MENU_USER);
    }
    else
    {
        logout();
    }
}

/*
 * @brief Cierra la sesion y vuelve al reposo sin dejar datos dando vueltas.
 */
static void logout (void)
{
    session = NULL;
    wipeSecrets();
    editLen = 0;
    optIdx  = 0;
    gotoState(ST_IDLE);
}

/*
 * @brief Alta de usuario (modo administrador): pendingId + el PIN recien
 * ingresado.
 */
static void addUser (void)
{
    uint8_t i;

    if (findUser(pendingId) != NULL)
    {
        wipeSecrets();
        showMessage("USEd", MSG_MS, ST_MENU, true, true);
        return;
    }

    for (i = 0; i < MAX_USERS; i++)
    {
        if (!users[i].used)
        {
            users[i].used      = true;
            users[i].isAdmin   = false;
            users[i].blocked   = false;
            users[i].failCount = 0;
            memCopy(users[i].id, pendingId, ID_LEN + 1);
            memCopy(users[i].pin, editBuf, PIN_MAX_LEN + 1);

            wipeSecrets();
            showMessage("donE", MSG_MS, ST_MENU, true, false);
            return;
        }
    }

    wipeSecrets();
    showMessage("FULL", MSG_MS, ST_MENU, true, true);
}

/*
 * @brief Baja de usuario (modo administrador).
 */
static void deleteUser (void)
{
    user_t* u = findUser(editBuf);

    if (u == NULL)
    {
        showMessage("nOnE", MSG_MS, ST_MENU, true, true);
        return;
    }

    if (u->isAdmin)
    {
        // Nunca se borra un administrador: si se borrara el ultimo, el equipo
        // quedaria sin forma de dar de alta a nadie mas.
        showMessage("Err", MSG_MS, ST_MENU, true, true);
        return;
    }

    memFill(u, 0, sizeof(user_t));
    showMessage("donE", MSG_MS, ST_MENU, true, false);
}

/*
 * @brief Desbloqueo de un ID que agoto sus intentos (modo administrador).
 */
static void unblockUser (void)
{
    user_t* u = findUser(editBuf);

    if (u == NULL)
    {
        showMessage("nOnE", MSG_MS, ST_MENU, true, true);
        return;
    }

    u->blocked   = false;
    u->failCount = 0;
    showMessage("donE", MSG_MS, ST_MENU, true, false);
}

/*
 * @brief Cambio de clave propia: compara la repeticion con la primera.
 */
static void changePin (void)
{
    if ((session == NULL) || (strCompare(pendingPin, editBuf) != 0))
    {
        wipeSecrets();
        showMessage("Err", MSG_MS, ST_MENU, true, true);
        return;
    }

    memCopy(session->pin, editBuf, PIN_MAX_LEN + 1);
    wipeSecrets();
    showMessage("donE", MSG_MS, ST_MENU, true, false);
}

/*------------------------------------------------------------------------------
 * Utilidades
 *----------------------------------------------------------------------------*/

static user_t* findUser (const char* id)
{
    uint8_t i;

    for (i = 0; i < MAX_USERS; i++)
    {
        if (users[i].used && (strCompare(users[i].id, id) == 0))
        {
            return &users[i];
        }
    }

    return NULL;
}

/*
 * @brief Saca el ID de 8 digitos del PAN leido de la tarjeta. Se toman los
 * ULTIMOS 8 digitos: los primeros identifican al banco y serian iguales para
 * todas las tarjetas de una misma entidad.
 * @return false si el PAN es corto o tiene algo que no sea un digito.
 */
static bool idFromPan (const char* pan, char out[ID_LEN + 1])
{
    size_t len = strLength(pan);
    size_t i;

    if (len < ID_LEN)
    {
        return false;
    }

    for (i = 0; i < ID_LEN; i++)
    {
        char c = pan[len - ID_LEN + i];

        if ((c < '0') || (c > '9'))
        {
            return false;
        }

        out[i] = c;
    }

    out[ID_LEN] = '\0';

    return true;
}

/*
 * @brief Borra de memoria lo que no deberia quedar dando vueltas.
 */
static void wipeSecrets (void)
{
    memFill(pendingPin, 0, sizeof(pendingPin));
    memFill(pendingId, 0, sizeof(pendingId));
    memFill(editBuf, 0, sizeof(editBuf));
}

/*
 * @brief Convierte las cuentas del encoder en pasos mecanicos enteros. Lo que
 * sobra queda pendiente para la proxima llamada, asi no se pierde giro ni se
 * arrastra error al cambiar de sentido.
 */
static int32_t takeDetents (void)
{
    int32_t raw     = Encoder_GetCount();
    int32_t delta   = raw - lastRawCount;
    int32_t detents = delta / ENCODER_COUNTS_PER_DETENT;

    lastRawCount += detents * ENCODER_COUNTS_PER_DETENT;

    return detents;
}

/*
 * @brief Milisegundos desde ts. La resta sin signo se banca el wrap de millis.
 */
static uint32_t elapsedSince (uint32_t ts)
{
    return millis() - ts;
}

/*
 * @brief Fase del parpadeo, comun a todo lo que titila.
 */
static bool blinkOn (void)
{
    return ((millis() / BLINK_MS) & 1u) == 0u;
}

/*
 * @brief Indice con vuelta redonda, tambien para valores negativos.
 */
static uint8_t wrapIndex (int32_t value, int32_t count)
{
    int32_t r = value % count;

    if (r < 0)
    {
        r += count;
    }

    return (uint8_t) r;
}

/*
 * @brief Copia src a un campo de MAX_CHARS, rellenando con espacios. Asi los
 * literales de texto no tienen que medir exactamente 4 caracteres.
 */
static void copyText (char dst[MAX_CHARS], const char* src)
{
    uint8_t i;
    bool    ended = false;

    for (i = 0; i < MAX_CHARS; i++)
    {
        if (!ended && (src[i] == '\0'))
        {
            ended = true;
        }

        dst[i] = ended ? ' ' : src[i];
    }
}

static void adjustBrightness (int32_t detents)
{
    int32_t level = (int32_t)brightness + detents;

    // Se satura en los extremos (no da la vuelta): apagar el display de golpe
    // por pasarse de rosca seria un mal chiste.
    if (level < BRIGHT_MIN)
    {
        level = BRIGHT_MIN;
    }
    if (level > BRIGHT_MAX)
    {
        level = BRIGHT_MAX;
    }

    brightness = (uint8_t) level;
    applyBrightness();
}

/*
 * @brief Aplica la intensidad elegida. El trabajo fino (repartir el slot de
 * cada digito entre encendido y apagado) es del driver: la app solo dice el
 * nivel, no sabe que haya un OE ni un barrido detras.
 */
static void applyBrightness (void)
{
    displaySetBrightness(brightness);
}

/*
 * @brief Prende o apaga un LED de la FRDM (son activos bajos).
 */
static void ledWrite (pin_t pin, bool on)
{
    gpioWrite(pin, on ? LED_ACTIVE : !LED_ACTIVE);
}

static void ledRgb (bool red, bool green, bool blue)
{
    // El lazo principal pasa por aca miles de veces por segundo: se escriben
    // los pines solo cuando el color realmente cambia.
    uint8_t rgb = (uint8_t)((red ? 4u : 0u) | (green ? 2u : 0u) | (blue ? 1u : 0u));

    if (rgb == lastRgb)
    {
        return;
    }

    lastRgb = rgb;

    ledWrite(PIN_LED_RED, red);
    ledWrite(PIN_LED_GREEN, green);
    ledWrite(PIN_LED_BLUE, blue);
}

/*******************************************************************************
 ******************************************************************************/
