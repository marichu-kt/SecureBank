#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#include "utils.h"

/*─────────────────────────────────────────────*/
/*            CONFIGURACIÓN GENERAL            */
/*─────────────────────────────────────────────*/
Config leer_config(const char *ruta) {
    Config c = {0};
    FILE *f = fopen(ruta, "r");
    if (!f) { perror("config.txt"); exit(EXIT_FAILURE); }

    char ln[128];
    while (fgets(ln, sizeof ln, f)) {
        if (ln[0]=='#' || strlen(ln)<3) continue;
        sscanf(ln, "LIMITE_RETIRO=%d",        &c.limite_retiro);
        sscanf(ln, "LIMITE_TRANSFERENCIA=%d", &c.limite_transferencia);
        sscanf(ln, "UMBRAL_RETIROS=%d",       &c.umbral_retiros);
        sscanf(ln, "UMBRAL_TRANSFERENCIAS=%d",&c.umbral_transferencias);
        sscanf(ln, "NUM_HILOS=%d",            &c.num_hilos);
        sscanf(ln, "ARCHIVO_CUENTAS=%49s",     c.archivo_cuentas);
        sscanf(ln, "ARCHIVO_LOG=%49s",         c.archivo_log);
    }
    fclose(f);
    return c;
}

/*─────────────────────────────────────────────*/
/*         LECTURA Y VOLCADO DE CUENTAS        */
/*─────────────────────────────────────────────*/

int cargar_cuentas(const char *ruta, Cuenta *cuentas) {
    FILE *fc = fopen(ruta, "rb");
    if (!fc) { perror("cuentas.dat"); exit(EXIT_FAILURE); }
    int n = fread(cuentas, sizeof(Cuenta), 100, fc);
    fclose(fc);
    return n;
}

void volcar_cuentas(const char *ruta, Cuenta *cuentas, int n) {
    FILE *fc = fopen(ruta, "wb");
    if (!fc) { perror("cuentas.dat (guardar)"); return; }
    fwrite(cuentas, sizeof(Cuenta), n, fc);
    fclose(fc);
}

/*─────────────────────────────────────────────*/
/*                 TIMESTAMP                   */
/*─────────────────────────────────────────────*/
void obtener_timestamp(char *dst, size_t n) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(dst, n, "[%Y-%m-%d %H:%M:%S]", &tm);
}

/*─────────────────────────────────────────────*/
/*                LOG CENTRAL                  */
/*─────────────────────────────────────────────*/

void append_log(const char *ruta_log, const char *linea) {
    FILE *lf = fopen(ruta_log, "a");
    if (!lf) { perror("log file"); return; }

    char ts[32];
    obtener_timestamp(ts, sizeof ts);
    fprintf(lf, "%s %s\n", ts, linea);
    fclose(lf);
}

/*─────────────────────────────────────────────*/
/*              LOG TRANSACCIÓN                */
/*─────────────────────────────────────────────*/
void log_transaccion_individual(int cuenta, const char *linea) {
    char ruta[160];
    snprintf(ruta, sizeof ruta, "transacciones/%d/transacciones.log", cuenta);

    mkdir("transacciones", 0777);
    char dir[128];
    snprintf(dir, sizeof dir, "transacciones/%d", cuenta);
    mkdir(dir, 0777);

    FILE *lf = fopen(ruta, "a");
    if (!lf) { perror("fopen log individual"); return; }

    char ts[32];
    obtener_timestamp(ts, sizeof ts);
    fprintf(lf, "%s %s\n", ts, linea);
    fclose(lf);
}