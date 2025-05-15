/* banco.c — Proceso principal de SecureBank (versión SHM + mutex P-S)
 *
 * 1. Reserva un segmento de memoria compartida con la tabla de cuentas.
 * 2. Inicializa un pthread_mutex_t en modo PTHREAD_PROCESS_SHARED para
 *    sincronizar a todos los procesos.
 * 3. Arranca el monitor y N procesos-usuario, pasándoles shm_id por argv.
 * 4. Al pulsar ENTER vuelca de nuevo la tabla a disco, destruye el mutex,
 *    libera la SHM y termina.
 */

/*  banco.c  – Proceso principal de SecureBank
 *  ▸ Crea la SHM con la tabla de cuentas + buffer de E/S
 *  ▸ Lanza monitor y n procesos-usuario en terminales aparte
 *  ▸ Hilo gestor que vacía el buffer al disco de manera asíncrona
 */

/*  banco.c  – Proceso “dispatcher” de SecureBank
 *
 *  ▸  Crea la SHM con la tabla de cuentas.
 *  ▸  Inicia un hilo IO que consume una cola-buffer de prioridad
 *       y sincroniza en disco sólo las cuentas modificadas.
 *  ▸  Lanza monitor + varios procesos-usuario en terminales.
 *  ▸  Volca la tabla a disco y libera recursos al terminar.
 *
 *  Compilar:   gcc -D_POSIX_C_SOURCE=200809L banco.c -o banco -pthread
 *  Ejecutar:   ./banco
 */
#define _POSIX_C_SOURCE 200809L          /* nanosleep(), strdup() … */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>                      /* execlp, nanosleep        */
#include <time.h>                        /* nanosleep                */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#define MAX_PROCESOS  100
#define BUF_CAP       64                 /* capacidad de la cola */

                /*────────── 1.  Estructuras en la SHM ──────────*/
typedef struct {
    int   numero_cuenta;
    char  titular[50];
    float saldo;
    int   bloqueado;                     /* 1 = bloqueada -- 0 = activa */
} Cuenta;

/* prioridad de la operación (extra parte 4) */
typedef enum { P_BAJA = 0, P_MEDIA = 1, P_ALTA = 2 } Prioridad;

typedef struct {
    Prioridad prio;
    Cuenta    snapshot;                  /* copia de la cuenta tras la op. */
} Operacion;

/* cola con orden de prioridad (productores: usuarios, consumidor: hilo IO) */
typedef struct {
    Operacion ops[BUF_CAP];
    int       n;                         /* elementos presentes (0..BUF_CAP) */
} BufferPrioridad;

typedef struct {
    Cuenta          cuentas[100];
    int             num_cuentas;
    pthread_mutex_t mutex;               /* protege la tabla y el buffer */
    BufferPrioridad buffer;              /* NUEVO: cola de prioridad     */
} TablaCuentas;

                /*────────── 2.  Config (config.txt) ──────────*/
typedef struct {
    int  limite_retiro;
    int  limite_transferencia;
    int  umbral_retiros;
    int  umbral_transferencias;
    int  num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

static Config leer_config(const char *ruta)
{
    Config c = {0};
    FILE *f = fopen(ruta, "r");
    if (!f) { perror("config.txt"); exit(EXIT_FAILURE); }

    char ln[128];
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0]=='#' || strlen(ln)<3) continue;
        sscanf(ln, "LIMITE_RETIRO=%d",           &c.limite_retiro);
        sscanf(ln, "LIMITE_TRANSFERENCIA=%d",    &c.limite_transferencia);
        sscanf(ln, "UMBRAL_RETIROS=%d",          &c.umbral_retiros);
        sscanf(ln, "UMBRAL_TRANSFERENCIAS=%d",   &c.umbral_transferencias);
        sscanf(ln, "NUM_HILOS=%d",               &c.num_hilos);
        sscanf(ln, "ARCHIVO_CUENTAS=%49s",        c.archivo_cuentas);
        sscanf(ln, "ARCHIVO_LOG=%49s",            c.archivo_log);
    }
    fclose(f);
    return c;
}

                /*────────── 3.  Hilo consumidor  ──────────*/
static void *gestionar_entrada_salida(void *arg)
{
    TablaCuentas      *t    = arg;
    const char *path        = getenv("SECUREBANK_FILE");    /* puesto por main */
    struct timespec pausa   = {0, 20000000L};               /* 20 ms */

    for (;;) {
        pthread_mutex_lock(&t->mutex);
        if (t->buffer.n == 0) {                 /* cola vacía → dormir un poco */
            pthread_mutex_unlock(&t->mutex);
            nanosleep(&pausa, NULL);
            continue;
        }
        /* sacar la operación de mayor prioridad (ya está ordenada) */
        Operacion op = t->buffer.ops[0];
        memmove(&t->buffer.ops[0], &t->buffer.ops[1],
                (t->buffer.n - 1) * sizeof(Operacion));
        --t->buffer.n;
        pthread_mutex_unlock(&t->mutex);

        /* localizar la cuenta y sincronizar sólo esa entrada */
        int idx = -1;
        for (int i = 0; i < t->num_cuentas; ++i)
            if (t->cuentas[i].numero_cuenta == op.snapshot.numero_cuenta) {
                idx = i; break;
            }
        if (idx == -1) continue;               /* imposible pero seguro */

        FILE *f = fopen(path, "rb+");
        if (!f) { perror("cuentas.dat (hilo IO)"); continue; }
        fseek(f, idx * sizeof(Cuenta), SEEK_SET);
        fwrite(&op.snapshot, sizeof(Cuenta), 1, f);
        fclose(f);
    }
    /* nunca retorna */
    return NULL;
}

                /*────────── 4.  Programa principal  ──────────*/
int main(void)
{
    /* 4.1 leer config */
    Config cfg = leer_config("config.txt");
    setenv("SECUREBANK_FILE", cfg.archivo_cuentas, 1);   /* visible al hilo */

    /* 4.2 crear SHM */
    int shm_id = shmget(IPC_PRIVATE, sizeof(TablaCuentas), IPC_CREAT | 0666);
    if (shm_id == -1) { perror("shmget"); exit(EXIT_FAILURE); }

    TablaCuentas *tabla = shmat(shm_id, NULL, 0);
    if (tabla == (void*)-1) { perror("shmat"); exit(EXIT_FAILURE); }

    /* 4.3 cargar cuentas desde disco */
    FILE *fc = fopen(cfg.archivo_cuentas, "rb");
    if (!fc) { perror("cuentas.dat"); exit(EXIT_FAILURE); }
    tabla->num_cuentas = fread(tabla->cuentas, sizeof(Cuenta), 100, fc);
    fclose(fc);

    /* mutex compartido */
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&tabla->mutex, &a);
    pthread_mutexattr_destroy(&a);

    /* buffer prioridad vacío */
    tabla->buffer.n = 0;

    /* 4.4 hilo IO asíncrono */
    pthread_t hilo_io;
    if (pthread_create(&hilo_io, NULL, gestionar_entrada_salida, tabla) != 0) {
        perror("pthread_create"); exit(EXIT_FAILURE);
    }

    /* 4.5 lanzar monitor + usuarios */
    pid_t pids[MAX_PROCESOS];
    int   n = 0;

    if ((pids[n] = fork()) == 0) {             /* monitor */
        execlp("gnome-terminal","gnome-terminal","--","bash","-c",
               "./monitor", (char*)NULL);
        perror("monitor"); _exit(EXIT_FAILURE);
    }
    ++n;

    struct timespec pausa = {0, 200000000L};   /* 0,2 s entre terminales */
    for (int i = 0; i < cfg.num_hilos && n < MAX_PROCESOS; ++i) {
        if ((pids[n] = fork()) == 0) {
            char cmd[64];
            snprintf(cmd, sizeof cmd, "./usuario %d", shm_id);
            execlp("gnome-terminal","gnome-terminal","--","bash","-c",
                   cmd,(char*)NULL);
            perror("usuario"); _exit(EXIT_FAILURE);
        }
        ++n;
        nanosleep(&pausa, NULL);
    }

    puts("Todos los procesos lanzados.  Pulse ENTER para cerrar…");
    getchar();

    /* 4.6 finalización limpia */
    for (int i = 0; i < n; ++i) kill(pids[i], SIGKILL);

    pthread_cancel(hilo_io);
    pthread_join(hilo_io, NULL);

    /* volcamos toda la tabla antes de salir */
    fc = fopen(cfg.archivo_cuentas, "wb");
    if (fc) {
        fwrite(tabla->cuentas, sizeof(Cuenta), tabla->num_cuentas, fc);
        fclose(fc);
    }

    pthread_mutex_destroy(&tabla->mutex);
    shmdt(tabla);
    shmctl(shm_id, IPC_RMID, NULL);

    puts("Sistema cerrado y recursos liberados.");
    return 0;
}
