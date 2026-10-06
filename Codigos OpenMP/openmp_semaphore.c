/*
BFS paralelo com OpenMP - Com exclusão mútua 

1. Compilar
---- gcc -O2 -fopenmp openmp_semaphore.c -o openmp_semaphore ----

2. Rodar

./executavel arquivoGrafo num_threads vertice_inicio1 vertice_inicio2 ...

---- ./openmp_semaphore grafo.csv 4 0 25 50 75 ----

*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <omp.h>

#define REPS 5        // num de repetições para tirar média de tempo

#define PRE_CHECK 1

static int n = 0;                              // num de vértices
static unsigned char *adj = NULL;              // matriz adj nxn
static volatile unsigned char *visited = NULL; 
static sem_t *sem = NULL;                    

// função para ler matriz de adj
static int ler_grafo(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("Erro ao abrir o arquivo"); return 0; }

    char *line = NULL;
    size_t cap = 0;
    ssize_t len = getline(&line, &cap, f);
    if (len <= 0) { fprintf(stderr, "Arquivo vazio.\n"); return 0; }

    for (ssize_t k = 0; k < len; k++)
        if (line[k] == '0' || line[k] == '1') n++;

    adj = malloc((size_t)n * n);
    visited = malloc(n);
    sem = malloc((size_t)n * sizeof(sem_t));
    if (!adj || !visited || !sem) { fprintf(stderr, "Sem memória.\n"); return 0; }

    rewind(f);
    for (int i = 0; i < n; i++) {
        len = getline(&line, &cap, f);
        if (len <= 0) { fprintf(stderr, "Faltam linhas (linha %d).\n", i); return 0; }
        int j = 0;
        for (ssize_t k = 0; k < len; k++)
            if ((line[k] == '0' || line[k] == '1') && j < n)
                adj[(size_t)i * n + j++] = line[k] - '0';
        if (j != n) { fprintf(stderr, "Linha %d com %d colunas (esperado %d).\n", i, j, n); return 0; }
    }
    free(line);
    fclose(f);

    for (int i = 0; i < n; i++) {
        if (sem_init(&sem[i], 0, 1) != 0) {
            perror("sem_init (no macOS use omp_lock_t no lugar de sem_t)");
            return 0;
        }
    }
    return 1;
}

// Tenta acessar vértice v. Retorna 1 se foi essa thread que acessou 
static int reivindicar(int v)
{
#if PRE_CHECK
    if (visited[v]) return 0;
#endif
    int ganhou = 0;
    sem_wait(&sem[v]);          // entra na seção crítica do vértice v 
    if (!visited[v]) {
        visited[v] = 1;
        ganhou = 1;
    }
    sem_post(&sem[v]);          // libera 
    return ganhou;
}

// BFS local das threads. Retorna quantos vértices a thread processou. 
static long bfs(int start)
{
    int *fila = malloc((size_t)n * sizeof(int));
    int head = 0, tail = 0;
    long contagem = 0;

    if (reivindicar(start))
        fila[tail++] = start;

    while (head < tail) {
        int u = fila[head++];
        contagem++;
        const unsigned char *linha = adj + (size_t)u * n;
        for (int v = 0; v < n; v++) {
            if (linha[v] && reivindicar(v))
                fila[tail++] = v;
        }
    }
    free(fila);
    return contagem;
}

typedef struct {
    double tempo;    // segundos
    long total;      // soma dos vértices processados 
    long unicos;     // vértices distintos visitados 
} Resultado;

static Resultado executar(int T, const int *starts)
{
    memset((void *)visited, 0, n);
    long total = 0;

    double t0 = omp_get_wtime();
    #pragma omp parallel num_threads(T) reduction(+:total)
    {
        total += bfs(starts[omp_get_thread_num()]);
    }
    double t1 = omp_get_wtime();

    long unicos = 0;
    for (int i = 0; i < n; i++) unicos += visited[i];

    Resultado r = { t1 - t0, total, unicos };
    return r;
}

static Resultado media(int T, const int *starts)
{
    Resultado m = { 0, 0, 0 };
    for (int r = 0; r < REPS; r++) {
        Resultado x = executar(T, starts);
        m.tempo += x.tempo;
        m.total += x.total;
        m.unicos += x.unicos;
    }
    m.tempo /= REPS;
    m.total /= REPS;
    m.unicos /= REPS;
    return m;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <grafo.csv> <num_threads> <v0> <v1> ... <v(T-1)>\n", argv[0]);
        return 1;
    }

    int T = atoi(argv[2]);
    if (T < 1) { fprintf(stderr, "num_threads deve ser >= 1\n"); return 1; }
    if (argc != 3 + T) {
        fprintf(stderr, "Foram pedidas %d threads, então são necessários %d vértices iniciais (recebi %d).\n",
                T, T, argc - 3);
        return 1;
    }

    if (!ler_grafo(argv[1])) return 1;

    int *starts = malloc(T * sizeof(int));
    for (int i = 0; i < T; i++) {
        starts[i] = atoi(argv[3 + i]);
        if (starts[i] < 0 || starts[i] >= n) {
            fprintf(stderr, "Vértice inicial %d inválido (grafo tem %d vértices: 0..%d).\n",
                    starts[i], n, n - 1);
            return 1;
        }
    }

    // parte sequencial
    int seq_start = starts[0];
    Resultado seq = media(1, &seq_start);

    // parte paralela
    Resultado par = media(T, starts);

    double speedup = seq.tempo / par.tempo;
    double eficiencia = speedup / T;

    printf("=== BFS paralelo (OpenMP, COM exclusao mutua: semaforo por vertice) ===\n");
    printf("Grafo: %s (%d vertices) | Threads: %d | Repeticoes: %d | PRE_CHECK: %d\n",
           argv[1], n, T, REPS, PRE_CHECK);
    printf("Vertices iniciais:");
    for (int i = 0; i < T; i++) printf(" T%d=%d", i, starts[i]);
    printf("\n\n");

    printf("Sequencial (1 thread, inicio em %d):\n", seq_start);
    printf("  Tempo:                  %.6f s\n", seq.tempo);
    printf("  Vertices visitados:     %ld\n\n", seq.unicos);

    printf("Paralelo (%d threads):\n", T);
    printf("  Tempo:                  %.6f s\n", par.tempo);
    printf("  Speedup:                %.4f\n", speedup);
    printf("  Eficiencia:             %.4f (%.2f%%)\n", eficiencia, eficiencia * 100.0);
    printf("  Vertices unicos:        %ld de %d\n", par.unicos, n);
    printf("  Vertices processados:   %ld (total somado das threads)\n", par.total);
    printf("  Trabalho redundante:    %ld\n", par.total - par.unicos);
    printf("  Vertices/s (unicos):    %.2f\n", par.unicos / par.tempo);
    printf("  Vertices/s (processados): %.2f\n", par.total / par.tempo);

    for (int i = 0; i < n; i++) sem_destroy(&sem[i]);
    free(sem);
    free(starts);
    free(adj);
    free((void *)visited);
    return 0;
}