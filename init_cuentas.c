/* init_cuentas.c  –  Genera cuentas.dat con la estructura vigente */

#include <stdio.h>
#include <stdlib.h>

/* ---------- Estructura actual de una cuenta ---------- */
typedef struct {
    int   numero_cuenta;
    char  titular[50];
    float saldo;
    int   bloqueado;          /* 0 = activa, 1 = bloqueada */
} Cuenta;

int main(void)
{
    FILE *f = fopen("cuentas.dat", "wb");
    if (!f) {
        perror("cuentas.dat");
        return 1;
    }

    /* Cuentas iniciales */
    Cuenta cuentas[] = {
        {1001, "John Doe",    5000.00f, 0},
        {1002, "Jane Smith",  3000.00f, 0},
        {1003, "Carlos Ruiz", 7000.00f, 0}
    };

    size_t total = sizeof(cuentas) / sizeof(cuentas[0]);
    size_t escritos = fwrite(cuentas, sizeof(Cuenta), total, f);
    fclose(f);

    if (escritos != total) {
        fprintf(stderr, "Error: solo se escribieron %zu de %zu cuentas\n",
                escritos, total);
        return 1;
    }

    puts("Archivo cuentas.dat creado con la estructura nueva.");
    return 0;
}
