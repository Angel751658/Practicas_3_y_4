/*           
        Para compilar incluir la librería  m (matemáticas)

        Ejemplo:
            gcc -o mercator mercator.c  -lm
*/

#include <fcntl.h>
#include <math.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000
#define SOLICITUD 1
#define RESPUESTA 2

typedef struct
{
    int tipo;
    double valor;
} Mensaje;

static const char *nombres[NPROCS] = {
    "/mercator_msg_0",
    "/mercator_msg_1",
    "/mercator_msg_2",
    "/mercator_msg_3"};

double get_member(int n, double x)
{
    double potencia = 1.0;

    for (int i = 0; i < n; i++)
        potencia *= x;

    return (n % 2 == 0) ? -potencia / n : potencia / n;
}

void worker(int id)
{
    mqd_t cola = mq_open(nombres[id], O_RDWR);
    Mensaje mensaje;
    double suma = 0.0;

    mq_receive(cola, (char *)&mensaje, sizeof(mensaje), NULL);

    for (int i = id; i < SERIES_MEMBER_COUNT; i += NPROCS)
        suma += get_member(i + 1, mensaje.valor);

    mensaje.tipo = RESPUESTA;
    mensaje.valor = suma;
    mq_send(cola, (const char *)&mensaje, sizeof(mensaje), 0);

    mq_close(cola);
    _exit(0);
}

void master(void)
{
    FILE *archivo = fopen("entrada.txt", "r");
    double x;
    mqd_t colas[NPROCS];
    Mensaje mensaje = {SOLICITUD, 0.0};
    double resultado = 0.0;
    struct timeval tiempo;
    long long inicio;
    long long fin;

    fscanf(archivo, "%lf", &x);
    fclose(archivo);
    mensaje.valor = x;

    for (int i = 0; i < NPROCS; i++)
        colas[i] = mq_open(nombres[i], O_RDWR);

    gettimeofday(&tiempo, NULL);
    inicio = tiempo.tv_sec * 1000000LL + tiempo.tv_usec;

    for (int i = 0; i < NPROCS; i++)
        mq_send(colas[i], (const char *)&mensaje, sizeof(mensaje), 0);

    for (int i = 0; i < NPROCS; i++)
    {
        mq_receive(colas[i], (char *)&mensaje, sizeof(mensaje), NULL);
        resultado += mensaje.valor;
    }

    gettimeofday(&tiempo, NULL);
    fin = tiempo.tv_sec * 1000000LL + tiempo.tv_usec;

    printf("Terminos calculados: %d\n", SERIES_MEMBER_COUNT);
    printf("Tiempo: %.6f segundos\n", (fin - inicio) / 1000000.0);
    printf("Resultado: %.10f\n", resultado);
    printf("Valor de referencia: %.10f\n", log(1.0 + x));

    for (int i = 0; i < NPROCS; i++)
        mq_close(colas[i]);

    exit(0);
}

int main(void)
{
    struct mq_attr atributos = {0};
    mqd_t colas[NPROCS];
    pid_t trabajadores[NPROCS];
    pid_t maestro;

    atributos.mq_maxmsg = 10;
    atributos.mq_msgsize = sizeof(Mensaje);

    for (int i = 0; i < NPROCS; i++)
    {
        mq_unlink(nombres[i]);
        colas[i] = mq_open(nombres[i], O_CREAT | O_RDWR, 0600, &atributos);
        mq_close(colas[i]);
    }

    for (int i = 0; i < NPROCS; i++)
    {
        trabajadores[i] = fork();

        if (trabajadores[i] == 0)
            worker(i);
    }

    maestro = fork();
    if (maestro == 0)
        master();

    waitpid(maestro, NULL, 0);
    for (int i = 0; i < NPROCS; i++)
        waitpid(trabajadores[i], NULL, 0);

    for (int i = 0; i < NPROCS; i++)
        mq_unlink(nombres[i]);

    return 0;
}