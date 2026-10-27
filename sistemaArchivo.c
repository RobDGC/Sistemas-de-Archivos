#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define NOMBRE_FS "fs-id_estudiante.bin"
#define TAM_FS (1024 * 1024)
#define TAM_BLOQUE 512
#define TAM_SUPERBLOQUE 512
#define TAM_TABLA_INODOS 4096
#define TAM_BITMAP 1024
#define TAM_INODO 64
#define MAX_INODOS (TAM_TABLA_INODOS / TAM_INODO)
#define MAX_NOMBRE 32
#define COMPROBAR 0x53464653

#define OFF_INODOS TAM_SUPERBLOQUE
#define OFF_BITMAP (OFF_INODOS + TAM_TABLA_INODOS)
#define OFF_DATOS (OFF_BITMAP + TAM_BITMAP)
#define TOTAL_BLOQUES ((TAM_FS - OFF_DATOS) / TAM_BLOQUE)

typedef struct {
    uint32_t magico;
    uint32_t tam_total;
    uint32_t tam_bloque;
    uint32_t total_bloques;
    uint32_t total_inodos;
    uint32_t bloques_libres;
    uint32_t inodos_libres;
    uint32_t off_inodos;
    uint32_t off_bitmap;
    uint32_t off_datos;
    uint8_t relleno[TAM_SUPERBLOQUE - 10 * sizeof(uint32_t)];
} SUPERBLOQUE;

typedef struct {
    char nombre[MAX_NOMBRE];
    uint32_t tam;
    int32_t bloque_inicial;
    uint32_t num_bloques;
    uint8_t usado;
    uint8_t relleno[19];
} INODO;

_Static_assert(sizeof(SUPERBLOQUE) == TAM_SUPERBLOQUE, "SUPERBLOQUE debe medir 512 bytes");
_Static_assert(sizeof(INODO) == TAM_INODO, "INODO debe medir 64 bytes");

/*
Nombre: leer_sb
Retorno: int
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    SUPERBLOQUE *sb: estructura donde se guardara la informacion del superbloque.
Funcion de la funcion:
    Lee el superbloque desde el inicio del archivo y devuelve 1 si la lectura fue exitosa o 0 si fallo.
*/
int leer_sb(FILE *archivo, SUPERBLOQUE *sb) {
    fseek(archivo, 0, SEEK_SET);
    return fread(sb, sizeof(SUPERBLOQUE), 1, archivo) == 1;
}

/*
Nombre: escribir_sb
Retorno: void
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    SUPERBLOQUE *sb: estructura con la informacion que se guardara.
Funcion de la funcion:
    Escribe el superbloque al inicio del archivo binario.
*/
void escribir_sb(FILE *archivo, SUPERBLOQUE *sb) {
    fseek(archivo, 0, SEEK_SET);
    fwrite(sb, sizeof(SUPERBLOQUE), 1, archivo);
}

/*
Nombre: leer_inodo
Retorno: void
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    int i: indice del inodo que se desea leer.
    INODO *in: estructura donde se guardaran los datos del inodo.
Funcion de la funcion:
    Lee el inodo indicado desde la tabla de inodos del archivo.
*/
void leer_inodo(FILE *archivo, int i, INODO *in) {
    fseek(archivo, OFF_INODOS + (long)i * TAM_INODO, SEEK_SET);
    fread(in, sizeof(INODO), 1, archivo);
}

/*
Nombre: escribir_inodo
Retorno: void
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    int i: indice del inodo que se desea escribir.
    INODO *in: estructura con los datos del inodo.
Funcion de la funcion:
    Escribe el inodo indicado en su posicion correspondiente dentro de la tabla de inodos.
*/
void escribir_inodo(FILE *archivo, int i, INODO *in) {
    fseek(archivo, OFF_INODOS + (long)i * TAM_INODO, SEEK_SET);
    fwrite(in, sizeof(INODO), 1, archivo);
}

/*
Nombre: leer_bitmap
Retorno: void
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    uint8_t *bm: arreglo donde se guardara el bitmap.
Funcion de la funcion:
    Lee el bitmap completo desde la posicion reservada para este en el archivo.
*/
void leer_bitmap(FILE *archivo, uint8_t *bm) {
    fseek(archivo, OFF_BITMAP, SEEK_SET);
    fread(bm, TAM_BITMAP, 1, archivo);
}

/*
Nombre: escribir_bitmap
Retorno: void
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    uint8_t *bm: arreglo con los datos del bitmap.
Funcion de la funcion:
    Escribe el bitmap completo en la posicion correspondiente del archivo.
*/
void escribir_bitmap(FILE *archivo, uint8_t *bm) {
    fseek(archivo, OFF_BITMAP, SEEK_SET);
    fwrite(bm, TAM_BITMAP, 1, archivo);
}

/*
Nombre: obtener_bit
Retorno: int
Parametros:
    uint8_t *bm: arreglo que contiene el bitmap.
    int b: indice del bloque cuyo estado se desea consultar.
Funcion de la funcion:
    Obtiene el valor del bit correspondiente al bloque indicado. Devuelve 1 si esta ocupado o 0 si esta libre.
*/
int obtener_bit(uint8_t *bm, int b) {
    return (bm[b / 8] >> (b % 8)) & 1;
}

/*
Nombre: establecer_bit
Retorno: void
Parametros:
    uint8_t *bm: arreglo que contiene el bitmap.
    int b: indice del bloque que se desea marcar.
Funcion de la funcion:
    Establece en 1 el bit correspondiente al bloque indicado para marcarlo como ocupado.
*/
void establecer_bit(uint8_t *bm, int b) {
    bm[b / 8] |= (1 << (b % 8));
}

/*
Nombre: limpiar_bit
Retorno: void
Parametros:
    uint8_t *bm: arreglo que contiene el bitmap.
    int b: indice del bloque que se desea liberar.
Funcion de la funcion:
    Establece en 0 el bit correspondiente al bloque indicado para marcarlo como libre.
*/
void limpiar_bit(uint8_t *bm, int b) {
    bm[b / 8] &= ~(1 << (b % 8));
}

/*
Nombre: buscar_inodo
Retorno: int
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
    const char *nombre: nombre del archivo que se desea buscar.
Funcion de la funcion:
    Recorre la tabla de inodos y devuelve el indice del archivo cuyo nombre coincide con el indicado. Devuelve -1 si no existe.
*/
int buscar_inodo(FILE *archivo, const char *nombre) {
    INODO in;
    for (int i = 0; i < MAX_INODOS; i++) {
        leer_inodo(archivo, i, &in);
        if (in.usado && strcmp(in.nombre, nombre) == 0)
            return i;
    }
    return -1;
}

/*
Nombre: buscar_inodo_libre
Retorno: int
Parametros:
    FILE *archivo: archivo binario del sistema de archivos.
Funcion de la funcion:
    Recorre la tabla de inodos y devuelve el indice del primer inodo libre. Devuelve -1 si todos estan ocupados.
*/
int buscar_inodo_libre(FILE *archivo) {
    INODO in;
    for (int i = 0; i < MAX_INODOS; i++) {
        leer_inodo(archivo, i, &in);
        if (!in.usado)
            return i;
    }
    return -1;
}

/*
Nombre: buscar_bloques_libres
Retorno: int
Parametros:
    uint8_t *bm: arreglo que contiene el bitmap.
    uint32_t n: cantidad de bloques consecutivos que se necesitan.
Funcion de la funcion:
    Busca en el bitmap una secuencia de bloques libres consecutivos y devuelve el indice del primer bloque de la secuencia. Devuelve -1 si no encuentra espacio suficiente.
*/
int buscar_bloques_libres(uint8_t *bm, uint32_t n) {
    uint32_t racha = 0;
    for (int b = 0; b < (int)TOTAL_BLOQUES; b++) {
        if (obtener_bit(bm, b)) {
            racha = 0;
        } else {
            racha++;
            if (racha == n)
                return b - (int)n + 1;
        }
    }
    return -1;
}

/*
Nombre: formatear_fs
Retorno: int
Parametros:
    Ninguno.
Funcion de la funcion:
    Crea o reinicia el archivo que representa el disco virtual, lo llena de ceros y escribe el superbloque y los inodos iniciales. Devuelve 0 si termina correctamente o -1 si ocurre un error.
*/
int formatear_fs(void) {
    FILE *archivo = fopen(NOMBRE_FS, "wb");
    if (!archivo) {
        perror("fopen");
        return -1;
    }

    uint8_t ceros[TAM_BLOQUE] = {0};
    for (int i = 0; i < TAM_FS / TAM_BLOQUE; i++)
        fwrite(ceros, TAM_BLOQUE, 1, archivo);

    SUPERBLOQUE sb = {0};
    sb.magico = COMPROBAR;
    sb.tam_total = TAM_FS;
    sb.tam_bloque = TAM_BLOQUE;
    sb.total_bloques = TOTAL_BLOQUES;
    sb.total_inodos = MAX_INODOS;
    sb.bloques_libres = TOTAL_BLOQUES;
    sb.inodos_libres = MAX_INODOS;
    sb.off_inodos = OFF_INODOS;
    sb.off_bitmap = OFF_BITMAP;
    sb.off_datos = OFF_DATOS;
    escribir_sb(archivo, &sb);

    INODO in = {0};
    in.bloque_inicial = -1;
    for (int i = 0; i < MAX_INODOS; i++)
        escribir_inodo(archivo, i, &in);

    fclose(archivo);
    printf("Sistema de archivos creado: %s (%d bytes, %d bloques de datos)\n",
           NOMBRE_FS, TAM_FS, (int)TOTAL_BLOQUES);
    return 0;
}

/*
Nombre: crear_fs
Retorno: int
Parametros:
    const char *nombre: nombre del archivo que se desea crear.
    const char *contenido: texto que se almacenara en el archivo.
Funcion de la funcion:
    Verifica el nombre, comprueba que no exista otro archivo con el mismo nombre, busca un inodo libre y asigna bloques consecutivos para guardar el contenido. Actualiza el bitmap y los contadores del superbloque. Devuelve 0 si tiene exito o -1 si ocurre un error.
*/
int crear_fs(const char *nombre, const char *contenido) {
    size_t tam_nombre = strlen(nombre);
    if (tam_nombre == 0 || tam_nombre >= MAX_NOMBRE) {
        fprintf(stderr, "Error: el nombre debe tener entre 1 y %d caracteres.\n",
                MAX_NOMBRE - 1);
        return -1;
    }

    FILE *archivo = fopen(NOMBRE_FS, "r+b");
    if (!archivo) {
        perror("fopen");
        return -1;
    }

    SUPERBLOQUE sb;
    if (!leer_sb(archivo, &sb)) {
        fprintf(stderr, "Error: no se pudo leer el superbloque.\n");
        fclose(archivo);
        return -1;
    }

    if (buscar_inodo(archivo, nombre) >= 0) {
        fprintf(stderr, "Error: ya existe un archivo llamado '%s'.\n", nombre);
        fclose(archivo);
        return -1;
    }

    int indice_inodo = buscar_inodo_libre(archivo);
    if (indice_inodo < 0) {
        fprintf(stderr, "Error: no quedan inodos libres (maximo %d archivos).\n",
                MAX_INODOS);
        fclose(archivo);
        return -1;
    }

    uint32_t tam = (uint32_t)strlen(contenido);
    uint32_t num_bloques = (tam + TAM_BLOQUE - 1) / TAM_BLOQUE;

    uint8_t bm[TAM_BITMAP];
    leer_bitmap(archivo, bm);

    int bloque_inicial = -1;
    if (num_bloques > 0) {
        bloque_inicial = buscar_bloques_libres(bm, num_bloques);
        if (bloque_inicial < 0) {
            fprintf(stderr,
                    "Error: no hay %u bloques contiguos libres (libres en total: %u).\n",
                    num_bloques, sb.bloques_libres);
            fclose(archivo);
            return -1;
        }

        uint8_t *buffer = calloc(num_bloques, TAM_BLOQUE);
        if (!buffer) {
            perror("calloc");
            fclose(archivo);
            return -1;
        }

        memcpy(buffer, contenido, tam);
        fseek(archivo, OFF_DATOS + (long)bloque_inicial * TAM_BLOQUE, SEEK_SET);
        fwrite(buffer, TAM_BLOQUE, num_bloques, archivo);
        free(buffer);

        for (uint32_t i = 0; i < num_bloques; i++)
            establecer_bit(bm, bloque_inicial + i);

        escribir_bitmap(archivo, bm);
    }

    INODO in = {0};
    strncpy(in.nombre, nombre, MAX_NOMBRE - 1);
    in.tam = tam;
    in.bloque_inicial = bloque_inicial;
    in.num_bloques = num_bloques;
    in.usado = 1;
    escribir_inodo(archivo, indice_inodo, &in);

    sb.bloques_libres -= num_bloques;
    sb.inodos_libres -= 1;
    escribir_sb(archivo, &sb);

    fclose(archivo);
    printf("Archivo '%s' creado (%u bytes, bloque inicial %d, %u bloque(s)).\n",
           nombre, tam, bloque_inicial, num_bloques);
    return 0;
}

/*
Nombre: listar_fs
Retorno: int
Parametros:
    Ninguno.
Funcion de la funcion:
    Recorre la tabla de inodos y muestra los archivos existentes, sus tamanos y sus bloques iniciales. Tambien presenta la cantidad de inodos y bloques libres. Devuelve 0 si termina correctamente o -1 si ocurre un error.
*/
int listar_fs(void) {
    FILE *archivo = fopen(NOMBRE_FS, "rb");
    if (!archivo) {
        perror("fopen");
        return -1;
    }

    SUPERBLOQUE sb;
    if (!leer_sb(archivo, &sb)) {
        fclose(archivo);
        return -1;
    }

    printf("%-32s %10s %15s\n", "NOMBRE", "TAMANO", "BLOQUE INICIAL");
    printf("%-32s %10s %15s\n", "------", "------", "--------------");

    int cantidad = 0;
    INODO in;
    for (int i = 0; i < MAX_INODOS; i++) {
        leer_inodo(archivo, i, &in);
        if (!in.usado)
            continue;

        if (in.bloque_inicial < 0)
            printf("%-32s %10u %15s\n", in.nombre, in.tam, "-");
        else
            printf("%-32s %10u %15d\n",
                   in.nombre, in.tam, in.bloque_inicial);

        cantidad++;
    }

    printf("\n%d archivo(s) | inodos libres: %u | bloques libres: %u de %u\n",
           cantidad, sb.inodos_libres, sb.bloques_libres, sb.total_bloques);

    fclose(archivo);
    return 0;
}

/*
Nombre: mostrar_fs
Retorno: int
Parametros:
    const char *nombre: nombre del archivo cuyo contenido se desea mostrar.
Funcion de la funcion:
    Busca el archivo por su nombre, lee su contenido desde los bloques de datos y lo muestra en la salida estandar. Devuelve 0 si termina correctamente o -1 si ocurre un error.
*/
int mostrar_fs(const char *nombre) {
    FILE *archivo = fopen(NOMBRE_FS, "rb");
    if (!archivo) {
        perror("fopen");
        return -1;
    }

    int indice_inodo = buscar_inodo(archivo, nombre);
    if (indice_inodo < 0) {
        fprintf(stderr, "Error: el archivo '%s' no existe.\n", nombre);
        fclose(archivo);
        return -1;
    }

    INODO in;
    leer_inodo(archivo, indice_inodo, &in);

    if (in.tam > 0) {
        char *buffer = malloc(in.tam);
        if (!buffer) {
            perror("malloc");
            fclose(archivo);
            return -1;
        }

        fseek(archivo, OFF_DATOS + (long)in.bloque_inicial * TAM_BLOQUE, SEEK_SET);
        size_t leidos = fread(buffer, 1, in.tam, archivo);
        fwrite(buffer, 1, leidos, stdout);
        free(buffer);
    }

    printf("\n");
    fclose(archivo);
    return 0;
}

/*
Nombre: eliminar_fs
Retorno: int
Parametros:
    const char *nombre: nombre del archivo que se desea eliminar.
Funcion de la funcion:
    Busca el archivo, libera sus bloques en el bitmap, borra sus datos, marca su inodo como libre y actualiza los contadores del superbloque. Devuelve 0 si termina correctamente o -1 si ocurre un error.
*/
int eliminar_fs(const char *nombre) {
    FILE *archivo = fopen(NOMBRE_FS, "r+b");
    if (!archivo) {
        perror("fopen");
        return -1;
    }

    SUPERBLOQUE sb;
    if (!leer_sb(archivo, &sb)) {
        fclose(archivo);
        return -1;
    }

    int indice_inodo = buscar_inodo(archivo, nombre);
    if (indice_inodo < 0) {
        fprintf(stderr, "Error: el archivo '%s' no existe.\n", nombre);
        fclose(archivo);
        return -1;
    }

    INODO in;
    leer_inodo(archivo, indice_inodo, &in);

    if (in.num_bloques > 0) {
        uint8_t bm[TAM_BITMAP];
        leer_bitmap(archivo, bm);

        for (uint32_t i = 0; i < in.num_bloques; i++)
            limpiar_bit(bm, in.bloque_inicial + i);

        escribir_bitmap(archivo, bm);

        uint8_t *ceros = calloc(in.num_bloques, TAM_BLOQUE);
        if (ceros) {
            fseek(archivo, OFF_DATOS + (long)in.bloque_inicial * TAM_BLOQUE, SEEK_SET);
            fwrite(ceros, TAM_BLOQUE, in.num_bloques, archivo);
            free(ceros);
        }
    }

    sb.bloques_libres += in.num_bloques;
    sb.inodos_libres += 1;

    memset(&in, 0, sizeof(in));
    in.bloque_inicial = -1;
    escribir_inodo(archivo, indice_inodo, &in);
    escribir_sb(archivo, &sb);

    fclose(archivo);
    printf("Archivo '%s' eliminado.\n", nombre);
    return 0;
}

/*
Nombre: existe_fs
Retorno: int
Parametros:
    Ninguno.
Funcion de la funcion:
    Comprueba si existe el archivo del disco virtual y si su superbloque contiene el valor de identificacion esperado. Devuelve 1 si el disco es valido o 0 si no existe o no es valido.
*/
int existe_fs(void) {
    FILE *archivo = fopen(NOMBRE_FS, "rb");
    if (!archivo)
        return 0;

    SUPERBLOQUE sb;
    int valido = leer_sb(archivo, &sb) && sb.magico == COMPROBAR;
    fclose(archivo);
    return valido;
}

/*
Nombre: uso
Retorno: void
Parametros:
    const char *programa: nombre con el que se ejecuto el programa.
Funcion de la funcion:
    Muestra las instrucciones de uso del programa y los comandos disponibles.
*/
void uso(const char *programa) {
    printf("Uso:\n");
    printf("  %s format                     Crea/reinicia el disco\n", programa);
    printf("  %s create <nombre> \"texto\"    Crea un archivo con ese contenido\n", programa);
    printf("  %s ls                         Lista los archivos\n", programa);
    printf("  %s cat <nombre>               Muestra el contenido\n", programa);
    printf("  %s delete <nombre>            Borra un archivo\n", programa);
}

/*
Nombre: main
Retorno: int
Parametros:
    int argc: cantidad de argumentos recibidos desde la linea de comandos.
    char *argv[]: arreglo con los argumentos recibidos.
Funcion de la funcion:
    Interpreta el comando solicitado, valida sus argumentos y comprueba que el disco exista cuando sea necesario. Ejecuta la funcion correspondiente y devuelve un codigo que indica si la operacion termino correctamente.
*/
int main(int argc, char *argv[]) {
    if (argc < 2) {
        uso(argv[0]);
        return 1;
    }

    const char *comando = argv[1];

    if (strcmp(comando, "format") == 0)
        return formatear_fs() == 0 ? 0 : 1;

    if (!existe_fs()) {
        fprintf(stderr,
                "Error: no hay disco valido. Ejecuta primero: %s format\n",
                argv[0]);
        return 1;
    }

    if (strcmp(comando, "create") == 0) {
        if (argc != 4) {
            fprintf(stderr, "Uso: %s create <nombre> \"texto\"\n", argv[0]);
            return 1;
        }
        return crear_fs(argv[2], argv[3]) == 0 ? 0 : 1;
    }

    if (strcmp(comando, "ls") == 0)
        return listar_fs() == 0 ? 0 : 1;

    if (strcmp(comando, "cat") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s cat <nombre>\n", argv[0]);
            return 1;
        }
        return mostrar_fs(argv[2]) == 0 ? 0 : 1;
    }

    if (strcmp(comando, "delete") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s delete <nombre>\n", argv[0]);
            return 1;
        }
        return eliminar_fs(argv[2]) == 0 ? 0 : 1;
    }

    fprintf(stderr, "Comando desconocido: %s\n", comando);
    uso(argv[0]);
    return 1;
}
