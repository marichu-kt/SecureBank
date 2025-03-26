#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

// Estructura para describir la operación
typedef struct {
    int tipo_operacion; // 1=depósito, 2=retiro, 3=transferencia, 4=consulta
    float monto;
    int cuenta_origen;
    int cuenta_destino; // solo se usa en transferencias
} DatosOperacion;

sem_t *sem_cuentas = NULL;

// Actualiza el archivo cuentas.dat
void actualizar_cuenta(int tipo_op, float monto, int cta_origen, int cta_destino) {
    // Entrar en sección crítica
    sem_wait(sem_cuentas);

    // Abrir el archivo binario
    FILE *f = fopen("cuentas.dat", "rb+");
    if(!f) {
        perror("Error al abrir cuentas.dat");
        sem_post(sem_cuentas);
        return;
    }

    // Leer las cuentas en un array temporal
    Cuenta arr[100];
    int total = fread(arr, sizeof(Cuenta), 100, f);
    if (total < 0) total = 0;

    // Buscar las cuentas origen/destino
    int idx_origen = -1, idx_destino = -1;
    for(int i=0; i<total; i++) {
        if(arr[i].numero_cuenta == cta_origen)
            idx_origen = i;
        if(arr[i].numero_cuenta == cta_destino)
            idx_destino = i;
    }

    // Realizar la operación según el tipo
    if (tipo_op == 1) {
        // Depósito
        if (idx_origen >= 0) {
            arr[idx_origen].saldo += monto;
            arr[idx_origen].num_transacciones++;
        }
    } else if (tipo_op == 2) {
        // Retiro
        if (idx_origen >= 0) {
            if (arr[idx_origen].saldo >= monto) {
                arr[idx_origen].saldo -= monto;
                arr[idx_origen].num_transacciones++;
            } else {
                printf("⚠️ Saldo insuficiente para el retiro.\n");
            }
        }
    } else if (tipo_op == 2) {
        // Retiro
        if (idx_origen >= 0) {
            if (arr[idx_origen].saldo >= monto) {
                arr[idx_origen].saldo -= monto;
                arr[idx_origen].num_transacciones++;
            } else {
                printf("⚠️ Saldo insuficiente para el retiro.\n");
            }
        }
    } else if (tipo_op == 3) {
        if (idx_origen >= 0 && idx_destino >= 0) {
            if (arr[idx_origen].saldo >= monto) {
                arr[idx_origen].saldo -= monto;
                arr[idx_origen].num_transacciones++;
                arr[idx_destino].saldo += monto;
                arr[idx_destino].num_transacciones++;
            } else {
                printf("⚠️ Saldo insuficiente para la transferencia.\n");
            }
        }
    }    

    // Recolocar el puntero y reescribir
    fseek(f, 0, SEEK_SET);
    fwrite(arr, sizeof(Cuenta), total, f);
    fclose(f);

    // Salir de sección crítica
    sem_post(sem_cuentas);
}

// Función que ejecuta el hilo
void* hilo_operacion(void *arg) {
    DatosOperacion *op = (DatosOperacion*) arg;
    actualizar_cuenta(op->tipo_operacion, op->monto, op->cuenta_origen, op->cuenta_destino);
    return NULL;
}

int main() {
    // Abrir semáforo nombrado (el mismo que creó banco.c)
    sem_cuentas = sem_open("/cuentas_sem", 0);
    if (sem_cuentas == SEM_FAILED) {
        perror("Error al abrir semaforo /cuentas_sem en usuario");
        exit(1);
    }

    while(1) {
        printf("\n=== Menu de Usuario ===\n");
        printf("1. Depósito\n");
        printf("2. Retiro\n");
        printf("3. Transferencia\n");
        printf("4. Consultar saldo\n");
        printf("5. Salir\n");
        printf("Seleccione una opción: ");

        int opcion;
        scanf("%d", &opcion);

        if(opcion < 1 || opcion > 5) {
            printf("Opción inválida.\n");
            continue;
        }
        if(opcion == 5) {
            printf("Saliendo...\n");
            break;
        }

        // Preparamos la estructura de datos
        DatosOperacion op;
        memset(&op, 0, sizeof(op));
        op.tipo_operacion = opcion;

        printf("Cuenta origen: ");
        scanf("%d", &op.cuenta_origen);

        if(opcion == 1) {
            // Depósito
            printf("Monto a depositar: ");
            scanf("%f", &op.monto);
        } else if(opcion == 2) {
            // Retiro
            printf("Monto a retirar: ");
            scanf("%f", &op.monto);
        } else if(opcion == 3) {
            // Transferencia
            printf("Cuenta destino: ");
            scanf("%d", &op.cuenta_destino);
            printf("Monto a transferir: ");
            scanf("%f", &op.monto);
        } else if(opcion == 4) {
            // Consulta (no pide monto)
        }

        // Crear hilo para la operación
        pthread_t tid;
        pthread_create(&tid, NULL, hilo_operacion, (void*)&op);
        pthread_join(tid, NULL);

        // Si es consulta, mostramos el saldo
        if(opcion == 4) {
            // Bloquear semáforo antes de leer
            sem_wait(sem_cuentas);
            FILE *f = fopen("cuentas.dat", "rb");
            if(!f) {
                perror("Error al abrir cuentas.dat para lectura");
                sem_post(sem_cuentas);
                continue;
            }
            Cuenta arr[100];
            int total = fread(arr, sizeof(Cuenta), 100, f);
            fclose(f);
            sem_post(sem_cuentas);

            int found = 0;
            for(int i=0; i<total; i++) {
                if(arr[i].numero_cuenta == op.cuenta_origen) {
                    printf("Saldo de la cuenta %d = %.2f\n",
                           arr[i].numero_cuenta, arr[i].saldo);
                    found = 1;
                    break;
                }
            }
            if(!found) {
                printf("Cuenta no encontrada.\n");
            }
        }
    }

    // Cerrar semáforo en este proceso
    sem_close(sem_cuentas);
    return 0;
}
