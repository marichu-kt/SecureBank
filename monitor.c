/* monitor.c — Proceso de supervisión de SecureBank
 * - Lee los mensajes que envían usuarios por la cola SYSV (clave 1234)
 * - Muestra la transacción por pantalla y la añade a transacciones.log
 * - Detecta patrones sencillos de fraude (retiros y transferencias repetitivas) */

#define _POSIX_C_SOURCE 200809L     /* strptime, etc.              */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ────────── Constantes ────────── */
#define MSG_KEY  1234
#define TAM_MAX  128

/* ────────── Configuración (sólo umbrales) ────────── */
typedef struct {
    int umbral_retiros;
    int umbral_transferencias;
    char archivo_log[64];
} Cfg;

static Cfg cfg;

/* ────────── Mensaje recibido ────────── */
struct msgbuf {
    long tipo;
    char texto[TAM_MAX];
};

/* ────────── Estado para la detección de anomalías ────────── */
static int  retiros_consecutivos[10000]      = {0};
static int  transferencias_rep[10000][10000] = {{0}};

/* ────────── Utilidades ────────── */
static void timestamp(char *dst, size_t n)
{
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(dst, n, "[%Y-%m-%d %H:%M:%S]", &tm);
}

static void leer_cfg(const char *ruta)
{
    FILE *f = fopen(ruta, "r");
    if (!f) { perror("config.txt"); exit(EXIT_FAILURE); }

    /* valores por defecto */
    cfg.umbral_retiros        = 3;
    cfg.umbral_transferencias = 5;
    strcpy(cfg.archivo_log, "transacciones.log");

    char ln[128];
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0] == '#' || strlen(ln) < 3) continue;
        sscanf(ln, "UMBRAL_RETIROS=%d",         &cfg.umbral_retiros);
        sscanf(ln, "UMBRAL_TRANSFERENCIAS=%d",  &cfg.umbral_transferencias);
        sscanf(ln, "ARCHIVO_LOG=%63s",           cfg.archivo_log);
    }
    fclose(f);
}

static void append_log(const char *linea)
{
    FILE *lf = fopen(cfg.archivo_log, "a");
    if (!lf) { perror("log file"); return; }

    char ts[32];
    timestamp(ts, sizeof ts);
    fprintf(lf, "%s %s\n", ts, linea);
    fclose(lf);
}

/* ────────── Reglas de anomalía ────────── */
static void analizar(const char *msg)
{
    int origen, destino;
    float monto;

    if (sscanf(msg, "RETIRO %d %f", &origen, &monto) == 2) {

        if (++retiros_consecutivos[origen] >= cfg.umbral_retiros) {
            char alerta[128];
            snprintf(alerta, sizeof alerta,
                     "ALERTA: %d retiros seguidos en cuenta %d",
                     cfg.umbral_retiros, origen);
            puts(alerta);
            append_log(alerta);
            retiros_consecutivos[origen] = 0;
        }

    } else if (sscanf(msg, "TRANSFERENCIA %d %d %f",
                      &origen, &destino, &monto) == 3) {

        if (++transferencias_rep[origen][destino] >= cfg.umbral_transferencias) {
            char alerta[160];
            snprintf(alerta, sizeof alerta,
                     "ALERTA: %d transferencias seguidas de %d a %d",
                     cfg.umbral_transferencias, origen, destino);
            puts(alerta);
            append_log(alerta);
            transferencias_rep[origen][destino] = 0;
        }

    } else if (sscanf(msg, "DEPOSITO %d %f", &origen, &monto) == 2) {
        /* reinicia contador de retiros cuando llega un depósito           */
        retiros_consecutivos[origen] = 0;
    }
}

/* ────────── main ────────── */
int main(void)
{
    leer_cfg("config.txt");

    int qid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (qid == -1) { perror("msgget"); exit(EXIT_FAILURE); }

    puts("Monitor activo. Esperando transacciones…");

    struct msgbuf m;

    for (;;) {
        if (msgrcv(qid, &m, sizeof m.texto, 0, 0) == -1) {
            if (errno == EINTR) continue;
            perror("msgrcv");
            break;
        }

        char ts[32];
        timestamp(ts, sizeof ts);
        printf("%s %s\n", ts, m.texto);

        append_log(m.texto);            /* log central */
        analizar(m.texto);              /* reglas de fraude */
    }

    return 0;
}
