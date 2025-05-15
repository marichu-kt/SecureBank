gcc init_cuentas.c -o init_cuentas
gcc banco.c -o banco -pthread -lrt
gcc usuario.c -o usuario -pthread -lrt
gcc monitor.c -o monitor -lrt
./init_cuentas
