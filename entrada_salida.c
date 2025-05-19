#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>  // para getenv

#include "utils.h"

/*─────────────────────────────────────────────*/
/*        FUNCIONES PARA GESTIÓN DE BUFFER      */
/*─────────────────────────────────────────────*/
void buffer_push(BufferPrioridad *b, const Cuenta *cta, Prioridad prio) {
    if (b->n >= BUF_CAP)
        return;

    int i = b->n++;
    while (i > 0 && b->ops[i-1].prio < prio) {
        b->ops[i] = b->ops[i-1];
        --i;
    }
    b->ops[i].prio     = prio;
    b->ops[i].snapshot = *cta;
}

/*─────────────────────────────────────────────*/
/*             HILO CONSUMIDOR IO               */
/*─────────────────────────────────────────────*/

void *gestionar_entrada_salida(void *arg) {
    TablaCuentas *t = arg;
    const char *path = getenv("SECUREBANK_FILE");
    struct timespec pausa = {0, 20000000L};  // 20 ms

    for (;;) {
        pthread_mutex_lock(&t->mutex);
        if (t->buffer.n == 0) {
            pthread_mutex_unlock(&t->mutex);
            nanosleep(&pausa, NULL);
            continue;
        }

        Operacion op = t->buffer.ops[0];
        memmove(&t->buffer.ops[0], &t->buffer.ops[1],
                (t->buffer.n - 1) * sizeof(Operacion));
        --t->buffer.n;
        pthread_mutex_unlock(&t->mutex);

        int idx = -1;
        for (int i = 0; i < t->num_cuentas; ++i) {
            if (t->cuentas[i].numero_cuenta == op.snapshot.numero_cuenta) {
                idx = i;
                break;
            }
        }
        if (idx == -1) continue;

        FILE *f = fopen(path, "rb+");
        if (!f) { perror("cuentas.dat (hilo IO)"); continue; }
        fseek(f, idx * sizeof(Cuenta), SEEK_SET);
        fwrite(&op.snapshot, sizeof(Cuenta), 1, f);
        fclose(f);
    }

    return NULL;
}
