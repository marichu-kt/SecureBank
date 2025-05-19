#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define BUF_CAP 64

typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int bloqueado;
} Cuenta;

typedef enum { P_BAJA = 0, P_MEDIA = 1, P_ALTA = 2 } Prioridad;

typedef struct {
    Prioridad prio;
    Cuenta snapshot;
} Operacion;

typedef struct {
    Operacion ops[BUF_CAP];
    int n;
} BufferPrioridad;

typedef struct {
    Cuenta cuentas[100];
    int num_cuentas;
    pthread_mutex_t mutex;
    BufferPrioridad buffer;
} TablaCuentas;

typedef struct {
    int limite_retiro;
    int limite_transferencia;
    int umbral_retiros;
    int umbral_transferencias;
    int num_hilos;
    char archivo_cuentas[50];
    char archivo_log[50];
} Config;

/* Memoria */
int crear_shm();
TablaCuentas* adjuntar_shm(int shm_id);
void liberar_shm(void *ptr, int shm_id);
void inicializar_mutex_proceso_compartido(pthread_mutex_t *mutex);
void destruir_mutex(pthread_mutex_t *mutex);

/* Ficheros */
Config leer_config(const char *ruta);
int cargar_cuentas(const char *ruta, Cuenta *cuentas);
void volcar_cuentas(const char *ruta, Cuenta *cuentas, int n);
void append_log(const char *ruta_log, const char *linea);
void log_transaccion_individual(int cuenta, const char *linea);
void obtener_timestamp(char *dst, size_t n);

/* Entrada/Salida */
void buffer_push(BufferPrioridad *b, const Cuenta *cta, Prioridad prio);
void *gestionar_entrada_salida(void *arg);


#endif
