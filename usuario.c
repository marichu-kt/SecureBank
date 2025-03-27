#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MSG_KEY 1234
#define TAM_MAX 128

typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

typedef struct {
    int tipo_operacion; // 1=depósito, 2=retiro, 3=transferencia, 4=consulta
    float monto;
    int cuenta_origen;
    int cuenta_destino;
} DatosOperacion;

struct msgbuf {
    long tipo;
    char texto[TAM_MAX];
};

sem_t *sem_cuentas = NULL;

// Función para enviar mensaje al monitor
void enviar_a_monitor(const char *mensaje) {
    int cola_id = msgget(MSG_KEY, 0666);
    if (cola_id == -1) {
        perror("Error al acceder a la cola de mensajes");
        return;
    }

    struct msgbuf msg;
    msg.tipo = 1;
    strncpy(msg.texto, mensaje, TAM_MAX);

    if (msgsnd(cola_id, &msg, sizeof(msg.texto), 0) == -1) {
        perror("Error al enviar mensaje al monitor");
    }
}

void actualizar_cuenta(int tipo_op, float monto, int cta_origen, int cta_destino) {
    sem_wait(sem_cuentas);
    FILE *f = fopen("cuentas.dat", "rb+");
    if (!f) {
        perror("Error al abrir cuentas.dat");
        sem_post(sem_cuentas);
        return;
    }

    Cuenta arr[100];
    int total = fread(arr, sizeof(Cuenta), 100, f);
    if (total < 0) total = 0;

    int idx_origen = -1, idx_destino = -1;
    for (int i = 0; i < total; i++) {
        if (arr[i].numero_cuenta == cta_origen)
            idx_origen = i;
        if (arr[i].numero_cuenta == cta_destino)
            idx_destino = i;
    }

    char buffer[TAM_MAX] = "";

    if (tipo_op == 1 && idx_origen >= 0) {
        arr[idx_origen].saldo += monto;
        arr[idx_origen].num_transacciones++;
        snprintf(buffer, TAM_MAX, "DEPOSITO %d %.2f", cta_origen, monto);
    } else if (tipo_op == 2 && idx_origen >= 0) {
        if (arr[idx_origen].saldo >= monto) {
            arr[idx_origen].saldo -= monto;
            arr[idx_origen].num_transacciones++;
            snprintf(buffer, TAM_MAX, "RETIRO %d %.2f", cta_origen, monto);
        } else {
            printf("⚠️ Saldo insuficiente para el retiro.\n");
        }
    } else if (tipo_op == 3 && idx_origen >= 0 && idx_destino >= 0) {
        if (arr[idx_origen].saldo >= monto) {
            arr[idx_origen].saldo -= monto;
            arr[idx_origen].num_transacciones++;
            arr[idx_destino].saldo += monto;
            arr[idx_destino].num_transacciones++;
            snprintf(buffer, TAM_MAX, "TRANSFERENCIA %d %d %.2f", cta_origen, cta_destino, monto);
        } else {
            printf("⚠️ Saldo insuficiente para la transferencia.\n");
        }
    }

    fseek(f, 0, SEEK_SET);
    fwrite(arr, sizeof(Cuenta), total, f);
    fclose(f);
    sem_post(sem_cuentas);

    // Enviar al monitor si es depósito, retiro o transferencia
    if (strlen(buffer) > 0) {
        enviar_a_monitor(buffer);
    }
}

void* hilo_operacion(void *arg) {
    DatosOperacion *op = (DatosOperacion*) arg;
    actualizar_cuenta(op->tipo_operacion, op->monto, op->cuenta_origen, op->cuenta_destino);
    return NULL;
}

int main() {
    sem_cuentas = sem_open("/cuentas_sem", 0);
    if (sem_cuentas == SEM_FAILED) {
        perror("Error al abrir semaforo /cuentas_sem en usuario");
        exit(1);
    }

    while (1) {
        printf("\n=== Menu de Usuario ===\n");
        printf("1. Depósito\n");
        printf("2. Retiro\n");
        printf("3. Transferencia\n");
        printf("4. Consultar saldo\n");
        printf("5. Salir\n");
        printf("Seleccione una opción: ");

        int opcion;
        scanf("%d", &opcion);

        if (opcion < 1 || opcion > 5) {
            printf("Opción inválida.\n");
            continue;
        }
        if (opcion == 5) {
            printf("Saliendo...\n");
            break;
        }

        DatosOperacion op = {0};
        op.tipo_operacion = opcion;

        printf("Cuenta origen: ");
        scanf("%d", &op.cuenta_origen);

        if (opcion == 1) {
            printf("Monto a depositar: ");
            scanf("%f", &op.monto);
        } else if (opcion == 2) {
            printf("Monto a retirar: ");
            scanf("%f", &op.monto);
        } else if (opcion == 3) {
            printf("Cuenta destino: ");
            scanf("%d", &op.cuenta_destino);
            printf("Monto a transferir: ");
            scanf("%f", &op.monto);
        }

        pthread_t tid;
        pthread_create(&tid, NULL, hilo_operacion, &op);
        pthread_join(tid, NULL);

        if (opcion == 4) {
            sem_wait(sem_cuentas);
            FILE *f = fopen("cuentas.dat", "rb");
            if (!f) {
                perror("Error al abrir cuentas.dat");
                sem_post(sem_cuentas);
                continue;
            }
            Cuenta arr[100];
            int total = fread(arr, sizeof(Cuenta), 100, f);
            fclose(f);
            sem_post(sem_cuentas);

            int found = 0;
            for (int i = 0; i < total; i++) {
                if (arr[i].numero_cuenta == op.cuenta_origen) {
                    printf("Saldo de la cuenta %d = %.2f\n", arr[i].numero_cuenta, arr[i].saldo);
                    found = 1;
                    break;
                }
            }
            if (!found)
                printf("Cuenta no encontrada.\n");
        }
    }

    sem_close(sem_cuentas);
    return 0;
}
