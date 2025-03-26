# Simulador de un Sistema Bancario Concurrente Avanzado (SecureBank)

![Banner SecureBank](https://raw.githubusercontent.com/marichu-kt/SecureBank/main/docs/securebank_banner.png)

**Asignatura:** Sistemas Operativos (Curso 2024-2025)  
**Práctica Final de Asignatura – Parte I**

## 👥 Integrantes del Grupo

| Nombre Completo   | Usuario GitHub            | Rol en el Proyecto                          |
|--------------------|----------------------------|----------------------------------------------|
| Mario Martínez     | [@marichu-kt](https://github.com/marichu-kt) | Desarrollador |
---

## Índice de Contenidos
1. [Introducción](#introducción)  
2. [Contexto del Problema](#contexto-del-problema)  
3. [Especificaciones Generales](#especificaciones-generales)  
   - [Programa Principal (banco.c)](#a-programa-principal-bancoc)  
   - [Procesos Hijos (Usuarios)](#b-procesos-hijos-usuarios)  
   - [Sincronización y Exclusión Mutua](#c-sincronización-y-exclusión-mutua)  
   - [Detección de Anomalías](#d-detección-de-anomalías)  
4. [Flujo Completo del Sistema](#flujo-completo-del-sistema)  
   - [Inicio del Sistema](#1-inicio-del-sistema)  
   - [Gestión de Usuarios](#2-gestión-de-usuarios)  
   - [Ejecución de Operaciones Bancarias](#3-ejecución-de-operaciones-bancarias)  
   - [Detección de Anomalías](#4-detección-de-anomalías)  
   - [Finalización del Sistema](#5-finalización-del-sistema)  
5. [Estructura del Sistema](#estructura-del-sistema)  
6. [Configuración del Sistema (config.txt)](#configuración-del-sistema-mediante-ficheros-de-parametrización)  

---

## Introducción

El proyecto **SecureBank** consiste en un **sistema bancario concurrente avanzado** desarrollado en C sobre Linux. El objetivo es **gestionar múltiples usuarios**, **realizar transacciones simultáneas** y **detectar patrones de actividad sospechosos** en tiempo real.

Se aprovechan conceptos clave de **Sistemas Operativos**:
- Creación y gestión de procesos (`fork()`)
- Comunicaciones entre procesos (tuberías, colas de mensajes, señales)
- Sincronización con **semáforos** y **mutex**
- Manejo de **hilos** (`pthread`)
- Estructuras y ficheros para almacenar cuentas y logs

---

## Contexto del Problema

Debes crear un **sistema bancario concurrente** que:
- **Cree procesos hijos** para gestionar cada usuario.
- **Use hilos** para manejar operaciones dentro de cada usuario (depósitos, retiros, transferencias).
- **Sincronice** el acceso a recursos compartidos (archivo de cuentas y logs) mediante semáforos y mutex.
- **Detecte y reporte** transacciones sospechosas en tiempo real.

Para ello, se propone **aprovechar características avanzadas de Linux**:  
- Tuberías (pipes)  
- Señales  
- Semáforos POSIX (`sem_open`)  
- Colas de mensajes (Message Queues)

El lenguaje C permitirá un **control de bajo nivel** y **acceso directo** al sistema operativo.

---

## Especificaciones Generales

### a) Programa Principal (banco.c)

1. **Inicializa** el sistema, administra las cuentas y coordina los procesos hijos.  
2. **Comunicación** con procesos hijo mediante tuberías y señales.  
3. **Gestiona** el log de transacciones y la creación del proceso **monitor** para las anomalías.

### b) Procesos Hijos (Usuarios)

1. Cada usuario es un proceso **independiente** con un menú interactivo: depósito, retiro, transferencia, consultar saldo, salir.  
2. Dentro de cada proceso hijo, se crean **hilos** para ejecutar las operaciones bancarias de forma concurrente.

### c) Sincronización y Exclusión Mutua

- **Semáforos** para controlar el acceso al archivo de cuentas (evitando escrituras simultáneas incorrectas).
- **Mutex** en caso de operaciones más finas que requieran exclusión mutua dentro del mismo proceso hijo.

### d) Detección de Anomalías

- Un proceso **monitor** independiente que analiza transacciones en tiempo real (por tuberías o colas de mensajes).
- Envía alertas cuando detecta:
  - Retiros consecutivos mayores a un monto límite.
  - Transferencias repetitivas entre las mismas cuentas.
  - Uso simultáneo de una cuenta desde varios procesos.

---

## Flujo Completo del Sistema

### 1. Inicio del Sistema

1. **Inicialización del archivo de cuentas** (con `init_cuentas.c`):  
   - Crea un archivo binario `cuentas.dat` con datos iniciales.  
   - Estructura recomendada:
     ```c
     struct Cuenta {
         int numero_cuenta;
         char titular[50];
         float saldo;
         int num_transacciones;
     };
     ```
   - Ejemplo en texto:
     ```
     1001,John Doe,5000.00,0
     1002,Jane Smith,3000.00,0
     ```
2. **Inicialización de semáforos**:  
   - Semáforo nombrado con:
     ```c
     sem_t *semaforo = sem_open("/cuentas_sem", O_CREAT, 0644, 1);
     ```
3. **Lanzamiento del proceso principal (`banco.c`)**:  
   - Espera conexiones de usuarios (por ejemplo en un bucle).  
   - Al recibir solicitud, hace `fork()` para crear el proceso hijo que ejecuta el menú de usuario.

### 2. Gestión de Usuarios

1. **Menú de Usuario**:  
   - Un bucle `while(1)` con `printf` y `scanf` para seleccionar operación:
     ```
     1. Depósito
     2. Retiro
     3. Transferencia
     4. Consultar saldo
     5. Salir
     ```
2. **Comunicación con el proceso principal**:  
   - Cada proceso hijo puede enviar detalles de la operación (tipo, monto, cuenta origen/destino, etc.) al proceso padre mediante tubería o cola de mensajes.

### 3. Ejecución de Operaciones Bancarias

1. **Hilos por operación**:  
   - Dentro del proceso hijo, se crea un `pthread_t` para manejar la operación.  
   - `pthread_create(&hilo, NULL, ejecutar_operacion, (void *)&datos_operacion);`
2. **Protección del archivo de cuentas**:  
   - Antes de leer/escribir:
     ```c
     sem_wait(semaforo);
     // Actualizar archivo
     sem_post(semaforo);
     ```
3. **Actualización de cuentas**:  
   - Abre `cuentas.dat` en modo `rb+`, busca la posición de la cuenta, y la reescribe con los cambios de saldo o transacciones.

### 4. Detección de Anomalías

1. **Proceso Monitor** (`monitor.c`):  
   - Se ejecuta independiente (otro `fork()`).
   - Lee transacciones usando colas de mensajes (`msgrcv`) u otro método.
2. **Identificación de patrones**:  
   - Retiros consecutivos altos.  
   - Muchas transferencias idénticas en poco tiempo.  
   - Uso simultáneo de la misma cuenta desde varios procesos.
3. **Alertas en tiempo real**:  
   - Escribe en tuberías:
     ```c
     write(pipe_monitor_hijos[1], "ALERTA: Transacción sospechosa en cuenta 1002\n", 50);
     ```

### 5. Finalización del Sistema

1. **Cierre controlado**:
   - Al terminar, destruir semáforo:
     ```c
     sem_unlink("/cuentas_sem");
     ```
2. **Generación del Log Final**:
   - El proceso principal registra operaciones en un archivo de log (ej. `transacciones.log`):
     ```
     [2024-12-01 12:00:00] Depósito en cuenta 1001: +1000.00
     [2024-12-01 12:01:00] Retiro en cuenta 1002: -500.00
     ```

---

## Estructura del Sistema

1. **banco.c**  
   Programa principal que coordina procesos hijos y el monitor.
2. **usuario.c**  
   Implementa el menú interactivo y la lógica de hilos para operaciones bancarias.
3. **monitor.c**  
   Realiza la detección de anomalías y envía alertas.
4. **init_cuentas.c**  
   Crea el archivo binario `cuentas.dat` con datos de ejemplo.

---

## Configuración del Sistema mediante Ficheros de Parametrización

### Archivo `config.txt`

Debe incluir parámetros como:

1. **Límites de operaciones**:
   - `LIMITE_RETIRO`: monto máximo por cada retiro.
   - `LIMITE_TRANSFERENCIA`: monto máximo por transferencia.
2. **Umbrales para detección de anomalías**:
   - `UMBRAL_RETIROS`: número de retiros consecutivos altos para generar alerta.
   - `UMBRAL_TRANSFERENCIAS`: número de transferencias repetitivas entre las mismas cuentas.
3. **Parámetros de ejecución**:
   - `NUM_HILOS`: número máximo de hilos simultáneos por proceso hijo.
   - `ARCHIVO_CUENTAS`: ruta al archivo binario de cuentas (ej. `cuentas.dat`).
   - `ARCHIVO_LOG`: ruta al archivo de log (ej. `transacciones.log`).

**Ejemplo de `config.txt`**:
```txt
# Límites de Operaciones
LIMITE_RETIRO=5000
LIMITE_TRANSFERENCIA=10000

# Umbrales de Detección de Anomalías
UMBRAL_RETIROS=3
UMBRAL_TRANSFERENCIAS=5

# Parámetros de Ejecución
NUM_HILOS=4
ARCHIVO_CUENTAS=cuentas.dat
ARCHIVO_LOG=transacciones.log
