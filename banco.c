#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>      // O_CREAT, O_EXCL
#include <sys/stat.h>   // S_IRUSR, S_IWUSR
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

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
    memset(&config, 0, sizeof(Config)); // Inicializa en 0
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        perror("Error al abrir config.txt");
        exit(1);
    }

    char linea[128];
    while (fgets(linea, sizeof(linea), archivo)) {
        if (linea[0] == '#' || strlen(linea) < 3) continue; // Ignorar comentarios y líneas vacías
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
    // 1. Leer la configuración
    Config config = leer_configuracion("config.txt");
    printf("Configuración leída:\n");
    printf("  LIMITE_RETIRO = %d\n", config.limite_retiro);
    printf("  LIMITE_TRANSFERENCIA = %d\n", config.limite_transferencia);
    printf("  UMBRAL_RETIROS = %d\n", config.umbral_retiros);
    printf("  UMBRAL_TRANSFERENCIAS = %d\n", config.umbral_transferencias);
    printf("  NUM_HILOS = %d\n", config.num_hilos);
    printf("  ARCHIVO_CUENTAS = %s\n", config.archivo_cuentas);
    printf("  ARCHIVO_LOG = %s\n", config.archivo_log);

    // 2. Crear semáforo nombrado
    sem_t *sem_cuentas = sem_open("/cuentas_sem", O_CREAT, 0644, 1);
    if (sem_cuentas == SEM_FAILED) {
        perror("No se pudo crear el semáforo /cuentas_sem");
        exit(1);
    }
    printf("Semáforo /cuentas_sem creado correctamente.\n");

    // 3. Lanzar proceso monitor
    pid_t pid_monitor = fork();
    if (pid_monitor < 0) {
        perror("Error al crear proceso monitor");
    } else if (pid_monitor == 0) {
        // Proceso hijo: monitor
        execl("./monitor", "./monitor", NULL);
        perror("Error al ejecutar monitor");
        exit(1);
    }

    // 4. Crear N procesos usuario (ejemplo: 2 usuarios)
    for (int i = 0; i < 2; i++) {
        pid_t pid_hijo = fork();
        if (pid_hijo < 0) {
            perror("Error en fork");
            exit(1);
        } else if (pid_hijo == 0) {
            // Proceso hijo -> usuario
            execl("./usuario", "./usuario", NULL);
            perror("Error al ejecutar usuario");
            exit(1);
        }
    }

    // 5. Esperar a que todos los hijos (monitor + usuarios) terminen
    for (int i = 0; i < 3; i++) {
        int status;
        wait(&status);
    }

    // 6. Cerrar y eliminar el semáforo
    if (sem_close(sem_cuentas) < 0) {
        perror("Error al cerrar semáforo");
    }
    if (sem_unlink("/cuentas_sem") < 0) {
        perror("Error al eliminar semáforo");
    }
    printf("Semáforo /cuentas_sem cerrado y eliminado.\n");

    printf("Proceso banco finalizado.\n");
    return 0;
}
