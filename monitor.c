#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MSG_KEY 1234
#define TAM_MAX 128

struct msgbuf {
    long tipo;
    char texto[TAM_MAX];
};

int retiros_consecutivos[10000] = {0};
int transferencias_repetidas[10000][10000] = {{0}};

int UMBRAL_RETIROS = 3;
int UMBRAL_TRANSFERENCIAS = 5;

// 📝 Función para guardar logs en archivo
void guardar_log(const char* texto) {
    FILE *f = fopen("transacciones.log", "a");
    if (f) {
        fprintf(f, "%s\n", texto);
        fclose(f);
    } else {
        perror("No se pudo abrir transacciones.log");
    }
}

void detectar_anomalia(const char* mensaje) {
    char tipo[32];
    int origen, destino;
    float monto;

    if (sscanf(mensaje, "RETIRO %d %f", &origen, &monto) == 2) {
        retiros_consecutivos[origen]++;
        if (retiros_consecutivos[origen] >= UMBRAL_RETIROS) {
            char alerta[128];
            snprintf(alerta, sizeof(alerta), "ALERTA: Retiros consecutivos en cuenta %d", origen);
            printf("🚨 %s\n", alerta);
            guardar_log(alerta);
            retiros_consecutivos[origen] = 0;
        }
    } else if (sscanf(mensaje, "TRANSFERENCIA %d %d %f", &origen, &destino, &monto) == 3) {
        transferencias_repetidas[origen][destino]++;
        if (transferencias_repetidas[origen][destino] >= UMBRAL_TRANSFERENCIAS) {
            char alerta[128];
            snprintf(alerta, sizeof(alerta), "ALERTA: Transferencias repetitivas de %d a %d", origen, destino);
            printf("🚨 %s\n", alerta);
            guardar_log(alerta);
            transferencias_repetidas[origen][destino] = 0;
        }
    } else {
        sscanf(mensaje, "DEPOSITO %d %f", &origen, &monto);
        retiros_consecutivos[origen] = 0;
    }
}

int main() {
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

        printf("📥 Transacción recibida: %s\n", mensaje.texto);
        guardar_log(mensaje.texto); // 🧾 Guardar cada transacción en el archivo log
        detectar_anomalia(mensaje.texto);
    }

    return 0;
}
