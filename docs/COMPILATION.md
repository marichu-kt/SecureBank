rm banco
rm monitor
rm usuario
rm init_cuentas
gcc banco.c memoria.c ficheros.c entrada_salida.c -o banco -pthread -lrt
gcc usuario.c memoria.c ficheros.c entrada_salida.c -o usuario -pthread -lrt
gcc monitor.c ficheros.c -o monitor
gcc init_cuentas.c -o init_cuentas
./init_cuentas
./banco
