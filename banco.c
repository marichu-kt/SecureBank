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

#include "utils.h"


#define MAX_PROCESOS  100
#define BUF_CAP       64                 /* capacidad de la cola */



                /*────────── 4.  Programa principal  ──────────*/
int main(void)
{
    /* 4.1 leer config */
    Config cfg = leer_config("config.txt");
    setenv("SECUREBANK_FILE", cfg.archivo_cuentas, 1);   /* visible al hilo */

    int shm_id = crear_shm();
    TablaCuentas *tabla = adjuntar_shm(shm_id);


    tabla->num_cuentas = cargar_cuentas(cfg.archivo_cuentas, tabla->cuentas);

    inicializar_mutex_proceso_compartido(&tabla->mutex);


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
        execlp("gnome-terminal","gnome-terminal","--","bash","-c","./monitor", (char*)NULL);
        perror("monitor"); _exit(EXIT_FAILURE);
    }
    ++n;

    struct timespec pausa = {0, 200000000L};   /* 0,2 s entre terminales */
    for (int i = 0; i < cfg.num_hilos && n < MAX_PROCESOS; ++i) {
        if ((pids[n] = fork()) == 0) {
            char cmd[64];
            snprintf(cmd, sizeof cmd, "./usuario %d", shm_id);
            execlp("gnome-terminal","gnome-terminal","--","bash","-c",cmd,(char*)NULL);
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

    volcar_cuentas(cfg.archivo_cuentas, tabla->cuentas, tabla->num_cuentas);


    destruir_mutex(&tabla->mutex);
    liberar_shm(tabla, shm_id);


    puts("Sistema cerrado y recursos liberados.");
    return 0;
}
