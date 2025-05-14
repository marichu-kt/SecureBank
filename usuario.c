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
    int tipo_operacion;
    float monto;
    int cuenta_origen;
    int cuenta_destino;
} DatosOperacion;

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

sem_t *sem_cuentas = NULL;
Config config;
int cuenta_sesion = -1;

Config leer_configuracion(const char *ruta) {
    Config config;
    memset(&config, 0, sizeof(Config));
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        perror("Error al abrir config.txt");
        exit(1);
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

void actualizar_cuenta(int tipo_op, float monto, int cta_destino) {
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
        if (arr[i].numero_cuenta == cuenta_sesion)
            idx_origen = i;
        if (arr[i].numero_cuenta == cta_destino)
            idx_destino = i;
    }

    char buffer[TAM_MAX] = "";

    if (idx_origen == -1) {
        printf("Error: La cuenta origen %d no existe.\n", cuenta_sesion);
        fclose(f);
        sem_post(sem_cuentas);
        return;
    }
    if (tipo_op == 3 && idx_destino == -1) {
        printf("Error: La cuenta destino %d no existe.\n", cta_destino);
        fclose(f);
        sem_post(sem_cuentas);
        return;
    }

    if (tipo_op == 1) {
        arr[idx_origen].saldo += monto;
        arr[idx_origen].num_transacciones++;
        snprintf(buffer, TAM_MAX, "DEPOSITO %d %.2f", cuenta_sesion, monto);
    } else if (tipo_op == 2) {
        if (arr[idx_origen].saldo >= monto) {
            arr[idx_origen].saldo -= monto;
            arr[idx_origen].num_transacciones++;
            snprintf(buffer, TAM_MAX, "RETIRO %d %.2f", cuenta_sesion, monto);
        } else {
            printf("Saldo insuficiente para el retiro.\n");
        }
    } else if (tipo_op == 3) {
        if (arr[idx_origen].saldo >= monto) {
            arr[idx_origen].saldo -= monto;
            arr[idx_origen].num_transacciones++;
            arr[idx_destino].saldo += monto;
            arr[idx_destino].num_transacciones++;
            snprintf(buffer, TAM_MAX, "TRANSFERENCIA %d %d %.2f", cuenta_sesion, cta_destino, monto);
        } else {
            printf("Saldo insuficiente para la transferencia.\n");
        }
    }

    fseek(f, 0, SEEK_SET);
    fwrite(arr, sizeof(Cuenta), total, f);
    fclose(f);
    sem_post(sem_cuentas);

    if (strlen(buffer) > 0) {
        enviar_a_monitor(buffer);
    }
}

int main() {
    config = leer_configuracion("config.txt");

    sem_cuentas = sem_open("/cuentas_sem", 0);
    if (sem_cuentas == SEM_FAILED) {
        perror("Error al abrir semaforo /cuentas_sem en usuario");
        exit(1);
    }

    // Inicio de sesión
    int cuenta_valida = 0;
    while (!cuenta_valida) {
        printf("\n╔═════════════════════════════╗\n");
        printf("║ INICIO DE SESIÓN DE USUARIO ║\n");
        printf("╚═════════════════════════════╝\n");
        printf("Introduce tu número de cuenta: ");
        
        scanf("%d", &cuenta_sesion);

        sem_wait(sem_cuentas);
        FILE *f = fopen("cuentas.dat", "rb");
        Cuenta arr[100];
        int total = fread(arr, sizeof(Cuenta), 100, f);
        fclose(f);
        sem_post(sem_cuentas);

        for (int i = 0; i < total; i++) {
            if (arr[i].numero_cuenta == cuenta_sesion) {
                cuenta_valida = 1;
                break;
            }
        }

        if (!cuenta_valida) {
            printf("Cuenta no encontrada. Inténtalo de nuevo.\n");
        }
    }

    while (1) {
        printf("\n╔══════════════════════════╗\n");
        printf("║    CAJERO AUTOMÁTICO     ║\n");
        printf("╠══════════════════════════╣\n");
        printf("║      CUENTA: %d        ║\n", cuenta_sesion);
        printf("╠══════════════════════════╣\n");
        printf("║ 1. Depósito              ║\n");
        printf("║ 2. Retiro                ║\n");
        printf("║ 3. Transferencia         ║\n");
        printf("║ 4. Consultar saldo       ║\n");
        printf("║ 5. Salir                 ║\n");
        printf("╚══════════════════════════╝\n");
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

        float monto;
        int cuenta_destino;

        if (opcion == 1) {
            printf("Monto a depositar: ");
            scanf("%f", &monto);
            actualizar_cuenta(1, monto, 0);
        } else if (opcion == 2) {
            printf("Monto a retirar: ");
            scanf("%f", &monto);
            if (monto > config.limite_retiro) {
                printf("Error: el retiro excede el límite permitido (%d).\n", config.limite_retiro);
                continue;
            }
            actualizar_cuenta(2, monto, 0);
        } else if (opcion == 3) {
            printf("Cuenta destino: ");
            scanf("%d", &cuenta_destino);
            printf("Monto a transferir: ");
            scanf("%f", &monto);
            if (monto > config.limite_transferencia) {
                printf("Error: la transferencia excede el límite permitido (%d).\n", config.limite_transferencia);
                continue;
            }
            actualizar_cuenta(3, monto, cuenta_destino);
        } else if (opcion == 4) {
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
                if (arr[i].numero_cuenta == cuenta_sesion) {
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
