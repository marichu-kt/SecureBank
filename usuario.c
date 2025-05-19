/* usuario.c — Proceso interactivo de SecureBank
 *
 *   • Recibe por argv[1] el shm_id que creó “banco”.
 *   • Hace shmat → obtiene puntero a TablaCuentas.
 *   • Todas las operaciones sobre la tabla se protegen con el
 *     mutex (PTHREAD_PROCESS_SHARED) que vive dentro de la SHM.
 *
 *   ¡Ya no usamos el semáforo POSIX “/cuentas_sem” ni tocamos
 *   cuentas.dat directamente!
 */
/* usuario.c — Terminal interactivo de SecureBank
 * - Accede a la tabla de cuentas en memoria compartida
 * - Protege la tabla con el mutex PTHREAD_PROCESS_SHARED (tabla->mutex)
 * - Registra cada operación en transacciones/<cuenta>/transacciones.log
 *   usando un semáforo POSIX nombrado (/log_<cuenta>)                    */

/*  usuario.c  — Productor de entradas en el buffer de E/S
 *  ▸ Actualiza la tabla de cuentas en SHM
 *  ▸ Empuja la cuenta modificada al buffer circular (BufferEstructurado)
 *  ▸ Registra transacciones en su log privado y avisa al monitor
 */

/* usuario.c — Proceso interactivo de SecureBank
 *   ● Accede a la tabla de cuentas en SHM
 *   ● Inserta cada operación en la cola de prioridad compartida
 *   ● Registra logs individuales y avisa al monitor
 *
 *  Compilar:  gcc -D_POSIX_C_SOURCE=200809L usuario.c -o usuario -lrt -pthread
 */
#define _POSIX_C_SOURCE 200809L      /* getline(), nanosleep … */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <semaphore.h>

#include "utils.h"


/* ──────────  Constantes  ────────── */
#define MSG_KEY   1234
#define TAM_MAX   128
#define BUF_CAP   64                 /* igual que en banco.c          */


/* ──────────  Mensaje a monitor  ────────── */
struct msgbuf {
    long tipo;
    char texto[TAM_MAX];
};

/* ──────────  Variables globales  ────────── */
static Config            cfg;
static TablaCuentas     *tabla = NULL;     /* SHM                     */
static pthread_mutex_t  *mtx   = NULL;     /* alias tabla->mutex      */
static int               cuenta_sesion = -1;

/* semáforo para el log del usuario */
static sem_t            *sem_log = NULL;
static char              sem_name[32];

/* ───────────────────────────────────────────── */
/*                 UTILIDADES                   */
/* ───────────────────────────────────────────── */


static void enviar_monitor(const char *txt)
{
    int q = msgget(MSG_KEY, 0666);
    if (q == -1) { perror("msgget"); return; }
    struct msgbuf m = { .tipo = 1 };
    strncpy(m.texto, txt, TAM_MAX - 1);
    msgsnd(q, &m, sizeof m.texto, 0);
}

static int buscar_cuenta(int num)
{
    for (int i = 0; i < tabla->num_cuentas; ++i)
        if (tabla->cuentas[i].numero_cuenta == num)
            return i;
    return -1;
}




/* ───────────────────────────────────────────── */
/*            OPERACIONES BANCARIAS              */
/* ───────────────────────────────────────────── */
static void deposito(float monto)
{
    pthread_mutex_lock(mtx);

    int idx = buscar_cuenta(cuenta_sesion);
    tabla->cuentas[idx].saldo += monto;

    buffer_push(&tabla->buffer, &tabla->cuentas[idx], P_ALTA);

    pthread_mutex_unlock(mtx);

    char buf[TAM_MAX];
    snprintf(buf, sizeof buf, "Depósito: +%.2f", monto);
    log_transaccion_individual(cuenta_sesion, buf);

    snprintf(buf, sizeof buf, "DEPOSITO %d %.2f", cuenta_sesion, monto);
    enviar_monitor(buf);
}

static void retiro(float monto)
{
    pthread_mutex_lock(mtx);

    int idx = buscar_cuenta(cuenta_sesion);
    if (tabla->cuentas[idx].saldo >= monto) {
        tabla->cuentas[idx].saldo -= monto;

        buffer_push(&tabla->buffer, &tabla->cuentas[idx], P_ALTA);

        pthread_mutex_unlock(mtx);

        char buf[TAM_MAX];
        snprintf(buf, sizeof buf, "Retiro: -%.2f", monto);
        log_transaccion_individual(cuenta_sesion, buf);


        snprintf(buf, sizeof buf, "RETIRO %d %.2f", cuenta_sesion, monto);
        enviar_monitor(buf);
    } else {
        pthread_mutex_unlock(mtx);
        puts("Saldo insuficiente.");
    }
}

static void transferencia(int destino, float monto)
{
    pthread_mutex_lock(mtx);

    int idx_o = buscar_cuenta(cuenta_sesion);
    int idx_d = buscar_cuenta(destino);
    if (idx_d == -1) { pthread_mutex_unlock(mtx);
                       puts("Cuenta destino no existe."); return; }

    if (tabla->cuentas[idx_o].saldo >= monto) {
        tabla->cuentas[idx_o].saldo -= monto;
        tabla->cuentas[idx_d].saldo += monto;

        buffer_push(&tabla->buffer, &tabla->cuentas[idx_o], P_ALTA);
        buffer_push(&tabla->buffer, &tabla->cuentas[idx_d], P_ALTA);


        pthread_mutex_unlock(mtx);

        char buf[TAM_MAX];
        snprintf(buf, sizeof buf, "Transferencia a %d: -%.2f", destino, monto);
        log_transaccion_individual(cuenta_sesion, buf);


        snprintf(buf, sizeof buf, "TRANSFERENCIA %d %d %.2f",
                 cuenta_sesion, destino, monto);
        enviar_monitor(buf);
    } else {
        pthread_mutex_unlock(mtx);
        puts("Saldo insuficiente.");
    }
}

static void consultar_saldo(void)
{
    pthread_mutex_lock(mtx);
    int idx = buscar_cuenta(cuenta_sesion);
    float s = tabla->cuentas[idx].saldo;

   buffer_push(&tabla->buffer, &tabla->cuentas[idx], P_ALTA);


    pthread_mutex_unlock(mtx);

    printf("Saldo actual = %.2f €\n", s);
}

/* ───────────────────────────────────────────── */
/*                  INTERFAZ TEXTO               */
/* ───────────────────────────────────────────── */
static void menu_operaciones(void)
{
    for (;;) {
        printf("\n╔════════════════════════════╗\n");
        printf("║   CAJERO  |  CUENTA %-6d ║\n", cuenta_sesion);
        printf("╠════════════════════════════╣\n");
        printf("║ 1. Depósito                ║\n");
        printf("║ 2. Retiro                  ║\n");
        printf("║ 3. Transferencia           ║\n");
        printf("║ 4. Consultar saldo         ║\n");
        printf("║ 5. Salir                   ║\n");
        printf("╚════════════════════════════╝\n");
        printf("Seleccione: ");

        int op; if (scanf("%d",&op)!=1) exit(0);
        if (op==5) break;

        float monto; int dest;
        switch (op) {
        case 1:
            printf("Monto a depositar: "); scanf("%f",&monto);
            deposito(monto);                break;
        case 2:
            printf("Monto a retirar: ");   scanf("%f",&monto);
            if (monto > cfg.limite_retiro)
                printf("Límite de retiro: %d\n", cfg.limite_retiro);
            else retiro(monto);
            break;
        case 3:
            printf("Cuenta destino: ");     scanf("%d",&dest);
            printf("Monto a transferir: "); scanf("%f",&monto);
            if (monto > cfg.limite_transferencia)
                printf("Límite de transferencia: %d\n",
                       cfg.limite_transferencia);
            else transferencia(dest,monto);
            break;
        case 4:
            consultar_saldo();              break;
        default:
            puts("Opción inválida.");
        }
    }
}

/* ───────────────────────────────────────────── */
/*                     main                      */
/* ───────────────────────────────────────────── */
int main(int argc,char *argv[])
{

    if (argc<2){ fprintf(stderr,"Uso: %s <shm_id>\n",argv[0]); exit(EXIT_FAILURE); }
    int shm_id = atoi(argv[1]);

    /* 1. Conectar a la SHM */
    tabla = adjuntar_shm(shm_id);
    mtx = &tabla->mutex;

    cfg = leer_config("config.txt");

    /* 2. Autenticación simple */
    while (1) {
        printf("\n╔═════════════════════════════╗\n");
        printf("║ INICIO DE SESIÓN DE USUARIO ║\n");
        printf("╚═════════════════════════════╝\n");
        printf("Introduce tu número de cuenta: ");
        if (scanf("%d",&cuenta_sesion)!=1) exit(0);

        pthread_mutex_lock(mtx);
        int idx = buscar_cuenta(cuenta_sesion);
        int ok  = idx!=-1 && tabla->cuentas[idx].bloqueado==0;
        pthread_mutex_unlock(mtx);

        if (ok) break;
        puts("Cuenta no válida o bloqueada.");
    }

    /* 3. Directorio/semáforo del log */
    if (mkdir("transacciones",0777)==-1 && errno!=EEXIST)
        perror("mkdir transacciones");

    char dir[128];
    snprintf(dir,sizeof dir,"transacciones/%d",cuenta_sesion);
    if (mkdir(dir,0777)==-1 && errno!=EEXIST)
        perror("mkdir cuenta");

    snprintf(sem_name,sizeof sem_name,"/log_%d",cuenta_sesion);
    sem_log = sem_open(sem_name,O_CREAT,0644,1);
    if (sem_log==SEM_FAILED){ perror("sem_open log"); exit(EXIT_FAILURE); }

    /* 4. Operaciones */
    menu_operaciones();

    /* 5. Limpieza */
    sem_close(sem_log);
    liberar_shm(tabla, -1);  // -1 indica que no liberamos shm_id (lo hace banco)
    return 0;
}
