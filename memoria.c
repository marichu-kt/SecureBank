#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>

#define BUF_CAP 64

typedef struct {
    int   numero_cuenta;
    char  titular[50];
    float saldo;
    int   bloqueado;
} Cuenta;

typedef enum { P_BAJA = 0, P_MEDIA = 1, P_ALTA = 2 } Prioridad;

typedef struct {
    Prioridad prio;
    Cuenta    snapshot;
} Operacion;

typedef struct {
    Operacion ops[BUF_CAP];
    int       n;
} BufferPrioridad;

typedef struct {
    Cuenta cuentas[100];
    int num_cuentas;
    pthread_mutex_t mutex;
    BufferPrioridad buffer;
} TablaCuentas;

/*─────────────────────────────────────────────*/
/*           FUNCIONES DE GESTIÓN DE SHM        */
/*─────────────────────────────────────────────*/

int crear_shm() {
    int shm_id = shmget(IPC_PRIVATE, sizeof(TablaCuentas), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    return shm_id;
}

TablaCuentas* adjuntar_shm(int shm_id) {
    TablaCuentas *tabla = shmat(shm_id, NULL, 0);
    if (tabla == (void*)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    return tabla;
}

void liberar_shm(void *ptr, int shm_id) {
    shmdt(ptr);
    shmctl(shm_id, IPC_RMID, NULL);
}

/*─────────────────────────────────────────────*/
/*            FUNCIONES DE MUTEX SHM           */
/*─────────────────────────────────────────────*/

void inicializar_mutex_proceso_compartido(pthread_mutex_t *mutex) 
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

void destruir_mutex(pthread_mutex_t *mutex) {
    pthread_mutex_destroy(mutex);
}
