#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int numero_cuenta;
    char titular[50];
    float saldo;
    int num_transacciones;
} Cuenta;

int main() {
    FILE *archivo = fopen("cuentas.dat", "wb");
    if (!archivo) {
        perror("No se pudo crear el archivo de cuentas");
        return 1;
    }

    Cuenta cuentas[] = {
        {1001, "John Doe", 5000.00, 0},
        {1002, "Jane Smith", 3000.00, 0},
        {1003, "Carlos Ruiz", 7000.00, 0}
    };

    fwrite(cuentas, sizeof(Cuenta), 3, archivo);
    fclose(archivo);

    printf("Archivo cuentas.dat creado con éxito.\n");
    return 0;
}
