#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

// Función para leer config.txt
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

int main() {
    Config config = leer_configuracion("config.txt");

    printf("Configuración leída:\n");
    printf("  LIMITE_RETIRO = %d\n", config.limite_retiro);
    printf("  LIMITE_TRANSFERENCIA = %d\n", config.limite_transferencia);
    printf("  UMBRAL_RETIROS = %d\n", config.umbral_retiros);
    printf("  UMBRAL_TRANSFERENCIAS = %d\n", config.umbral_transferencias);
    printf("  NUM_HILOS = %d\n", config.num_hilos);
    printf("  ARCHIVO_CUENTAS = %s\n", config.archivo_cuentas);
    printf("  ARCHIVO_LOG = %s\n", config.archivo_log);

    // Crear semáforo nombrado
    sem_t *sem_cuentas = sem_open("/cuentas_sem", O_CREAT, 0644, 1);
    if (sem_cuentas == SEM_FAILED) {
        perror("No se pudo crear el semáforo /cuentas_sem");
        exit(EXIT_FAILURE);
    }
    printf("Semáforo /cuentas_sem creado correctamente.\n");

    printf("⏳ Esperando que usuarios y monitor se ejecuten por separado...\n");
    printf("Cuando desee cerrar el sistema, presione ENTER...\n");
    getchar();

    // Cerrar y eliminar semáforo al salir
    if (sem_close(sem_cuentas) < 0)
        perror("Error al cerrar semáforo");

    if (sem_unlink("/cuentas_sem") < 0)
        perror("Error al eliminar semáforo");

    printf("Semáforo /cuentas_sem cerrado y eliminado.\n");
    printf("Proceso banco finalizado.\n");

    return 0;
}
