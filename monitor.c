#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>

#define MSG_KEY 1234
#define TAM_MAX 128

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

struct msgbuf {
    long tipo;
    char texto[TAM_MAX];
};

int retiros_consecutivos[10000] = {0};
int transferencias_repetidas[10000][10000] = {{0}};
Config config;

void obtener_timestamp(char* buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", &tm);
}

Config leer_configuracion(const char *ruta) {
    Config config;
    memset(&config, 0, sizeof(Config));
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        perror("Error al abrir config.txt");
        exit(EXIT_FAILURE);
    }

    char linea[128];
    while (fgets(linea, sizeof(linea), archivo)) {
        if (linea[0] == '#' || strlen(linea) < 3) continue;
        if (strstr(linea, "LIMITE_RETIRO"))
            sscanf(linea, "LIMITE_RETIRO=%d", &config.limite_retiro);
        else if (strstr(linea, "LIMITE_TRANSFERENCIA"))
            sscanf(linea, "LIMITE_TRANSFERENCIA=%d", &config.limite_transferencia);
        else if (strstr(linea, "UMBRAL_RETIROS"))
            sscanf(linea, "UMBRAL_RETIROS=%d", &config.umbral_retiros);
        else if (strstr(linea, "UMBRAL_TRANSFERENCIAS"))
            sscanf(linea, "UMBRAL_TRANSFERENCIAS=%d", &config.umbral_transferencias);
        else if (strstr(linea, "NUM_HILOS"))
            sscanf(linea, "NUM_HILOS=%d", &config.num_hilos);
        else if (strstr(linea, "ARCHIVO_CUENTAS"))
            sscanf(linea, "ARCHIVO_CUENTAS=%s", config.archivo_cuentas);
        else if (strstr(linea, "ARCHIVO_LOG"))
            sscanf(linea, "ARCHIVO_LOG=%s", config.archivo_log);
    }

    fclose(archivo);
    return config;
}

void guardar_log(const char* texto) {
    FILE *f = fopen(config.archivo_log, "a");
    if (f) {
        char timestamp[64];
        obtener_timestamp(timestamp, sizeof(timestamp));
        fprintf(f, "%s %s\n", timestamp, texto);
        fclose(f);
    } else {
        perror("No se pudo abrir el archivo de log");
    }
}

void detectar_anomalia(const char* mensaje) {
    int origen, destino;
    float monto;

    if (sscanf(mensaje, "RETIRO %d %f", &origen, &monto) == 2) {
        retiros_consecutivos[origen]++;
        if (retiros_consecutivos[origen] >= config.umbral_retiros) {
            char alerta[128], timestamp[64];
            snprintf(alerta, sizeof(alerta), "ALERTA: Retiros consecutivos en cuenta %d", origen);
            obtener_timestamp(timestamp, sizeof(timestamp));
            printf("%s %s\n", timestamp, alerta);
            guardar_log(alerta);
            retiros_consecutivos[origen] = 0;
        }
    } else if (sscanf(mensaje, "TRANSFERENCIA %d %d %f", &origen, &destino, &monto) == 3) {
        transferencias_repetidas[origen][destino]++;
        if (transferencias_repetidas[origen][destino] >= config.umbral_transferencias) {
            char alerta[128], timestamp[64];
            snprintf(alerta, sizeof(alerta), "ALERTA: Transferencias repetitivas de %d a %d", origen, destino);
            obtener_timestamp(timestamp, sizeof(timestamp));
            printf("%s %s\n", timestamp, alerta);
            guardar_log(alerta);
            transferencias_repetidas[origen][destino] = 0;
        }
    } else if (sscanf(mensaje, "DEPOSITO %d %f", &origen, &monto) == 2) {
        retiros_consecutivos[origen] = 0; // reset
    }
}

int main() {
    config = leer_configuracion("config.txt");

    key_t clave = MSG_KEY;
    int cola_id;
    struct msgbuf mensaje;

    cola_id = msgget(clave, IPC_CREAT | 0666);
    if (cola_id == -1) {
        perror("Error al crear/acceder a la cola");
        exit(1);
    }

    printf("Monitor iniciado. Esperando transacciones...\n");

    while (1) {
        if (msgrcv(cola_id, &mensaje, sizeof(mensaje.texto), 0, 0) == -1) {
            perror("Error al recibir mensaje");
            exit(1);
        }

        char timestamp[64];
        obtener_timestamp(timestamp, sizeof(timestamp));
        printf("%s Transacción recibida: %s\n", timestamp, mensaje.texto);

        guardar_log(mensaje.texto);
        detectar_anomalia(mensaje.texto);
    }

    return 0;
}
