#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    printf("Monitor iniciado.\n");
    // Simulación de un bucle donde monitorea transacciones
    // (En la práctica real, usaría msgget(), msgrcv(), pipes, etc.)
    for(int i=0; i<5; i++){
        printf("Monitor corriendo... (simulacion)\n");
        sleep(2);
    }
    printf("Monitor finalizado.\n");
    return 0;
}
