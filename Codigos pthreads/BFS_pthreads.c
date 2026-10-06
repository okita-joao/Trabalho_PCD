#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <windows.h>
#include <time.h>
#include <sys/time.h>


// ===========================================================================//
//                              VARIÁVEIS GLOBAIS                             //
// ===========================================================================//

// Caminho do arquivo contendo a matriz original
char caminho[1024] = "grafo_10000x10000.csv";

char caminho_relatorio[1024] = "relatorio.txt";

// 1 = Aleatório | 0 = Arbitrário
#define V_INICIAIS_RND 1

#define NUM_THREADS 100

int *vertices_iniciais;

// Declaração global da barreira de incialização do algoritmo
pthread_barrier_t barreira_inicializacao;

// Declaração global do mutex para prints
pthread_mutex_t trava_print;


// ===========================================================================//
//         CRIAÇÃO DE ESTRUTURAS AUXILIARES E SEUS RESPECTIVOS MÉTODOS        //
// ===========================================================================//

typedef struct celula{
    void *conteudo;
    struct celula *prox;
} Celula;

typedef struct{
    Celula *primeiro;
    Celula *ultimo;
    int num_elementos;
} Fila;

void fila_push(Fila *F, void *conteudo_novo){
    if(F == NULL){
        printf("Fila inexistente!");
        return ;
    }
    
    Celula *nova = (Celula*)malloc(sizeof(Celula));
    if(nova == NULL){
        printf("Falha ao alocar memória para célula!\n");
        return ;
    }
    
    nova->conteudo = conteudo_novo;
    nova->prox = NULL;
    
    if(F->primeiro == NULL && F->ultimo == NULL){
        F->primeiro = nova;
        F->ultimo = nova;
    }
    else{
        F->ultimo->prox = nova;
        F->ultimo = nova;
    }

    F->num_elementos++;
}

Celula* fila_pop(Fila *F){
    Celula *saida = NULL;
    if(F != NULL && F->primeiro != NULL && F->ultimo != NULL){
        saida = F->primeiro;
        
        if(F->primeiro == F->ultimo){
            F->primeiro = NULL;
            F->ultimo = NULL;
        }
        else{
            F->primeiro = F->primeiro->prox;
        }

        F->num_elementos--;
        
        saida->prox = NULL;
    }
    
    return saida;
}

void fila_destruir(Fila *F){
    if(F != NULL && F->primeiro != NULL && F->ultimo != NULL){
        for(int i = 0; i < F->num_elementos; i++)
            fila_pop(F);
    }
}

// ===========================================================================//
//          CRIAÇÃO DE ESTRUTURAS AUXILIARES PARA EXPLORAÇÃO NO GRAFO         //
// ===========================================================================//

typedef struct{
    int index_vertice;
    int dist;
    int index_pai;
    int index_thread_visita;
} Vertice;

// ===========================================================================//
//          CRIAÇÃO DA ESTRUTURA DO GRAFO E SEUS RESPECTIVOS MÉTODOS          //
// ===========================================================================//

typedef struct{
    int num_vertices;
    int num_arestas;
    int **matriz_adj;
} Grafo;

// Função que lê uma linha inteira, de qualquer tamanho
char* ler_linha(FILE *fp){
    size_t cap = 256, len = 0;
    char *linha = (char*)malloc(cap);
    if(linha == NULL) return NULL;

    while(fgets(linha + len, (int)(cap - len), fp) != NULL){
        len += strlen(linha + len);

        if(linha[len - 1] == '\n') break;   // linha completa
        if(len < cap - 1) break;            // fim do arquivo sem '\n'

        // Buffer encheu sem achar '\n': dobra a capacidade e continua
        char *tmp = (char*)realloc(linha, cap * 2);
        if(tmp == NULL){
            free(linha);
            return NULL;
        }
        linha = tmp;
        cap *= 2;
    }

    if(len == 0){   // nada lido (EOF)
        free(linha);
        return NULL;
    }
    return linha;
}

// Delimitadores dos campos do CSV ('\r' incluído: no Linux o fopen("r") não converte \r\n)
#define DELIMITADORES ", \r\n"

// Função que conta os campos da primeira linha, ou seja, o número de vértices do grafo
static int contar_campos(const char *linha){
    char *copia = (char*)malloc(strlen(linha) + 1);
    if(copia == NULL) return -1;
    strcpy(copia, linha);                 // strtok modifica a string, por isso a cópia

    int n = 0;
    for(char *t = strtok(copia, DELIMITADORES); t != NULL; t = strtok(NULL, DELIMITADORES))
        n++;

    free(copia);
    return n;
}

// Função que libera um grafo, inclusive quando ele está parcialmente construído
void grafo_destruir(Grafo *g){
    if(g == NULL) return;

    if(g->matriz_adj != NULL){
        for(int i = 0; i < g->num_vertices; i++)
            free(g->matriz_adj[i]);
        free(g->matriz_adj);
    }
    free(g);
}

// Função que lê um grafo de um arquivo externo:
Grafo* instancia_grafo(char caminho[1024]){
    FILE  *fp = NULL;
    char  *linha = NULL;
    Grafo *g = NULL;
    int    n = 0;

    fp = fopen(caminho, "r");
    if(fp == NULL){
        printf("Não foi possível abrir o arquivo %s\n", caminho);
        goto erro;
    }

    // Criando o grafo
    g = (Grafo*)calloc(1, sizeof(Grafo));    // num_vertices = 0 e matriz_adj = NULL
    if(g == NULL){
        printf("Erro ao alocar memória para o grafo.\n");
        goto erro;
    }

    // Lendo a primeira linha da matriz do arquivo
    linha = ler_linha(fp);
    if(linha == NULL){
        printf("Arquivo vazio ou erro de leitura.\n");
        goto erro;
    }

    // Contando o número de vértices
    n = contar_campos(linha);
    if(n <= 0){
        printf("Contagem de campos deu errado, retorno %d!\n", n);
        goto erro;
    }

    // Atualizando os atributos do grafo
    g->matriz_adj = (int**)calloc(n, sizeof(int*));
    if(g->matriz_adj == NULL){
        printf("Erro ao alocar memória para a matriz de adjacências.\n");
        goto erro;
    }
    g->num_vertices = n;    // só depois de matriz_adj existir, para o grafo_destruir ser seguro

    // Instanciando a matriz de adjacência do grafo
    for(int i = 0; i < n; i++){
        g->matriz_adj[i] = (int*)calloc(n, sizeof(int));
        if(g->matriz_adj[i] == NULL){
            printf("Erro ao alocar memória na matriz de adjacências.\n");
            goto erro;
        }
    }

    int m = 0;

    // Preenchendo a matriz (a primeira linha já foi lida)
    for(int i = 0; i < n; i++){
        if(i > 0){
            linha = ler_linha(fp);
            if(linha == NULL){
                printf("Arquivo com %d linha(s), esperado %d.\n", i, n);
                goto erro;
            }
        }

        int j = 0;
        for(char *t = strtok(linha, DELIMITADORES); t != NULL && j < n; t = strtok(NULL, DELIMITADORES)){
            int valor = atoi(t);

            // Contando o número de arestas
            if(j >= i && valor == 1)
                m++;

            g->matriz_adj[i][j++] = valor;
        }

        if(j < n){
            printf("Linha %d com %d campo(s), esperado %d.\n", i + 1, j, n);
            goto erro;
        }

        free(linha);
        linha = NULL;       // evita double free no bloco de erro
    }

    fclose(fp);

    // Atualiza o número de arestas do grafo passado no arquivo externo
    g->num_arestas = m;

    return g;

erro:
    free(linha);
    if(fp != NULL) fclose(fp);
    grafo_destruir(g);
    return NULL;
}

void print_grafo(Grafo *g){
    printf("Grafo:\n");
    printf("Numero de Vertices = %d\nNumero de arestas = %d\n", g->num_vertices, g->num_arestas);
    
    printf("Matriz de Adjacencia:\n");
    for(int i = 0; i < g->num_vertices; i++){
        for(int j = 0; j < g->num_vertices; j++){
            printf("%d ", g->matriz_adj[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}


// ===========================================================================//
//                ALGORITMOS DE EXPLORAÇÃO EM GRAFOS (BDS & DFS)              //
// ===========================================================================//

typedef struct{
    Grafo *g;
    int index_thread;
    int index_vertice_origem;
    int *cinzas;
    int *pretos;
    pthread_mutex_t *mutexes_vertices;
    Vertice *resultados;
} arg;

void* BFS(void *argumento){
    arg dado = *(arg *)argumento;
    Grafo *g = dado.g;
    int index_thread = dado.index_thread;
    int index_vertice_origem = dado.index_vertice_origem;
    int *pretos = dado.pretos;
    int *cinzas = dado.cinzas;
    pthread_mutex_t *mutexes_vertices = dado.mutexes_vertices;
    Vertice *resultados = dado.resultados;

    Vertice *vertices = resultados;
    
    Fila *Q = (Fila*)malloc(sizeof(Fila));
    if(Q == NULL){
        printf("Erro ao alocar memória para a fila de vértices.\n");

        // Barreira para que todas as threads iniciem ao mesmo tempo
        pthread_barrier_wait(&barreira_inicializacao);
        return NULL;
    }
    
    Q->primeiro = NULL;
    Q->ultimo = NULL;
    Q->num_elementos = 0;
    
    void *p = &index_vertice_origem;
    fila_push(Q, p);
    
    vertices[index_vertice_origem].dist = 0;
    vertices[index_vertice_origem].index_thread_visita = index_thread;
    cinzas[index_vertice_origem] = 1;

    pthread_mutex_lock(&trava_print);

    printf("\n+ Thread %d preparada para iniciar!\n", index_thread);

    pthread_mutex_unlock(&trava_print);

    // Barreira para que todas as threads iniciem ao mesmo tempo
    pthread_barrier_wait(&barreira_inicializacao);
    
    // Exploração do grafo em Largura
    int *pa;
    while(Q->primeiro != NULL && Q->ultimo != NULL){
        Celula *u = fila_pop(Q);
        if(u == NULL){
            printf("Erro ao explorar o grafo pelo BFS.\n");

            // Liberação da memória alocada para a fila Q
            fila_destruir(Q);

            return NULL;
        }
        
        int u_index = *(int *)u->conteudo;
        for(int i = 0; i < g->num_vertices; i++){

            if(g->matriz_adj[u_index][i] != 0){
                pthread_mutex_lock(&mutexes_vertices[i]);

                if(cinzas[i] == 0){
                    int *v_index = (int*)malloc(sizeof(int));
                    if(v_index == NULL){
                        printf("Erro ao alocar memoria para um inteiro.\n");

                        // Liberação da memória alocada para a Célula *u
                        free(u->conteudo); 
                        free(u);

                        // Liberação da memória alocada para a fila Q até o momento
                        fila_destruir(Q);

                        pthread_mutex_unlock(&mutexes_vertices[i]);
                        return NULL;
                    }
                    *v_index = i;
                    
                    fila_push(Q, v_index);
                    
                    cinzas[i] = 1;
                    vertices[i].index_pai = u_index;
                    vertices[i].index_thread_visita = index_thread;
                    vertices[i].dist = vertices[u_index].dist + 1;
                }

                pthread_mutex_unlock(&mutexes_vertices[i]);
            }
        }
        
        pretos[u_index] = 1;

        if(u->conteudo != &index_vertice_origem)
            free(u->conteudo);
            
        free(u);
    }
    
    free(Q);

    pthread_exit(NULL);
}

// ===========================================================================//
//                    IMPLEMENTAÇÃO DAS THREADS VIA PTHREADS                  //
// ===========================================================================//

Vertice* BFS_multithread(Grafo *g, struct timeval *inicio, struct timeval *fim){
    if(g == NULL || g->num_vertices <= 0){
        printf("Grafo inexistente!\n");
        return NULL;
    }

    // 1. Criar as threads para a execução, e atribuir para cada uma
    //    o grafo e seus respectivos argumentos.
    
    pthread_t *threads = (pthread_t*)malloc(NUM_THREADS*sizeof(pthread_t));
    if(threads == NULL){
        printf("Erro ao alocar memória para o vetor de threads.\n");
        return NULL;
    }

    Vertice *resultados = (Vertice*)malloc(g->num_vertices*sizeof(Vertice));
    if(resultados == NULL){
        printf("Erro ao alocar memória para o vetor de resultados.\n");
        free(threads);
        return NULL;
    }

    for(int i = 0; i < g->num_vertices; i++){
        resultados[i].dist = -1;
        resultados[i].index_pai = i;
        resultados[i].index_vertice = i;
        resultados[i].index_thread_visita = -1;
    }

    // Criando vetor global para cinzas e pretas do processo
    int *pretos = (int*)calloc(g->num_vertices, sizeof(int));
    int *cinzas = (int*)calloc(g->num_vertices, sizeof(int));

    if(pretos == NULL || cinzas == NULL){
        printf("Erro ao alocar memória para o vetor de pretos ou cinzas.\n");
        free(threads);
        free(resultados);
        if(pretos != NULL) free(pretos);
        else free(cinzas);

        return NULL;
    }

    // Criando vetor de mutexes associados a cada vértice do grafo
    // Obs.: Medida posteriormente implementada para resolver a condição de corrida sobre a visita a cada um dos vértices do grafo
    pthread_mutex_t *mutexes_vertices = (pthread_mutex_t*)malloc(g->num_vertices*sizeof(pthread_mutex_t));
    if(mutexes_vertices == NULL){
        printf("Erro ao alocar memória para o vetor de mutexes dos vértices.\n");
        free(threads);
        free(resultados);
        free(cinzas);
        free(pretos);
        return NULL;
    }

    for(int i = 0; i < g->num_vertices; i++){
        pthread_mutex_init(&mutexes_vertices[i], NULL);
    }

    // Iniciando a barreira de inicialização e o mutex de prints
    pthread_barrier_init(&barreira_inicializacao, NULL, NUM_THREADS + 1);
    pthread_mutex_init(&trava_print, NULL);

    arg *argumentos = (arg*)calloc(NUM_THREADS, sizeof(arg));
    if(argumentos == NULL){
        printf("Erro ao alocar memória para o vetor de argumentos das threads.\n");
        free(threads);
        free(resultados);
        free(cinzas);
        free(pretos);
        pthread_barrier_destroy(&barreira_inicializacao);
        pthread_mutex_destroy(&trava_print);
        for(int i = 0; i < g->num_vertices; i++){
            pthread_mutex_destroy(&mutexes_vertices[i]);
        }
        free(mutexes_vertices);
        return NULL;
    }

    for(int i = 0; i < NUM_THREADS; i++){
        arg *argumento = &argumentos[i];

        argumento->g = g;
        argumento->index_thread = i;
        argumento->index_vertice_origem = vertices_iniciais[i];
        argumento->cinzas = cinzas;
        argumento->pretos = pretos;
        argumento->mutexes_vertices = mutexes_vertices;
        argumento->resultados = resultados;

        pthread_create(&threads[i], NULL, BFS, (void*)argumento);
    }

    // 2. Medição do tempo de execução.

    pthread_barrier_wait(&barreira_inicializacao);

    gettimeofday(inicio, NULL);

    printf("\nBarreira liberada, threads em acao!\n");

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    gettimeofday(fim, NULL);

    double tempo_gasto = ((*fim).tv_sec - (*inicio).tv_sec) + ((*fim).tv_usec - (*inicio).tv_usec) / 1000000.0;

    printf("\nTempo gasto: %f\n", tempo_gasto);

    // 3. Limpeza de memória
    free(cinzas);
    free(pretos);
    free(threads);

    pthread_barrier_destroy(&barreira_inicializacao);
    pthread_mutex_destroy(&trava_print);

    for(int i = 0; i < g->num_vertices; i++){
        pthread_mutex_destroy(&mutexes_vertices[i]);
    }
    free(mutexes_vertices);

    free(argumentos);

    // 4. Retorno da função
    return resultados;
}

// ===========================================================================//
//                              FUNÇÕES AUXILIARES                            //
// ===========================================================================//

int* gera_vertices_iniciais(int num_vertices, int num_threads){
    int *vetor = (int*)malloc(num_threads*sizeof(int));
    if(vetor == NULL){
        printf("Erro ao alocar memória para o vetor de vértices iniciais.\n");
        return NULL;
    }

    // Inicializa a semente com o tempo atual
    srand(time(NULL));

    // Gera e imprime um número aleatório entre 0 e o número de vértices informado
    int flag = 0, numero;

    for(int i = 0; i < num_threads; i++){
        flag = 0;

        while(1){
            numero = rand() % num_vertices;

            for(int j = 0; j < i; j++){
                if(vetor[j] == numero)
                    flag = 1;
            }

            if(flag == 1){
                flag = 0;
                continue;
            }
            
            break;
        }
        
        vetor[i] = numero;
    }

    return vetor;
}


void gera_relatorio(Grafo *g, Vertice *resultados, struct timeval *i, struct timeval *f){

    FILE *arquivo = fopen(caminho_relatorio, "w");

    if(arquivo == NULL){
        printf("Caminho invalido ou erro de permissao.\n");
        return ;
    }

    struct timeval inicio = *i, fim = *f;

    double tempo_gasto = (fim.tv_sec - inicio.tv_sec) + (fim.tv_usec - inicio.tv_usec) / 1000000.0;

    fprintf(arquivo, "Busca em Largura (BFS) com Multithreads:\n\nGrafo explorado: %s\n\nNúmero de vértices = %d\n\nNúmero de arestas = %d\n\nNUM_THREADS = %d\n", caminho, g->num_vertices, g->num_arestas, NUM_THREADS);

    if(V_INICIAIS_RND == 1){
        fprintf(arquivo, "\nVertices iniciais escolhidos aleatoriamente: ");
    }
    else{
        fprintf(arquivo, "\nVertices iniciais escolhidos: ");
    }
    
    for(int i = 0; i < NUM_THREADS - 1; i++)
        fprintf(arquivo, "%d, ", vertices_iniciais[i]);
    fprintf(arquivo, "%d\n", vertices_iniciais[NUM_THREADS - 1]);

    fprintf(arquivo, "\nTempo gasto: %f\n", tempo_gasto);

    if(resultados == NULL){
        fprintf(arquivo, "ERRO NA FUNÇÃO BFS_multithread.\n");
    }
    else{
        fprintf(arquivo, "\nLista de vertices:\n");
        for(int i = 0; i < g->num_vertices; i++){
            Vertice v = resultados[i];
            int t = v.index_thread_visita;
            fprintf(arquivo, "\nVertice (%d):\nIndex do vertice pai: %d\nDistancia: %d\nThread que o visitou: %d - (Origem: Vertice (%d))\n", i, v.index_pai, v.dist, t, vertices_iniciais[t]);
        }
    }

    fclose(arquivo);
}

// ===========================================================================//
//                              PROGRAMA PRINCIPAL                            //
// ===========================================================================//


int main(){
    
    struct timeval inicio, fim;

    Grafo *g = instancia_grafo(caminho);

    if(g == NULL){
        printf("Erro ao criar o grafo g, dado o arquivo %s\n", caminho);
        return -1;
    }

    if(g->num_vertices < NUM_THREADS){
        printf("O numero de threads não pode ser maior do que o número de vertices do grafo!\n");
        return -1;
    }

    // Caso a flag V_INICIAIS_RND == 1, então os vértices iniciais do grafo são gerados aleatóriamente
    if(V_INICIAIS_RND == 1){
        // Geração aleatória dos vértices iniciais
        vertices_iniciais = gera_vertices_iniciais(g->num_vertices, NUM_THREADS);
    }
    
    // Caso contrário, isso seja definido pelo usuário, então apenas tem-se uma verificação se tais vértices são válidos
    else if(V_INICIAIS_RND == 0){
        int *confirma = (int*)calloc(g->num_vertices, sizeof(int));
        if(confirma == NULL){
            printf("Erro ao alocar memória para o vetor confirma de inteiros.\n");
            return -1;
        }

        for(int i = 0; i< g->num_vertices; i++){
            if(vertices_iniciais[i] >= g->num_vertices || confirma[i] == 1){
                printf("Vetor de vertices iniciais invalido!\n");
            }
            else{
                confirma[vertices_iniciais[i]] = 1;
            }
        }

        free(confirma);
    }

    // Caso de saída
    else{
        printf("Erro na leitura da variavel global V_INICIAIS_RND, pois esta nao assume valor 0 ou 1.\n");
        return -1;
    }

    printf("Começando...\n");
    
    // Rodando a Busca em Largura:
    Vertice *resultados = BFS_multithread(g, &inicio, &fim);

    if(resultados == NULL){
        printf("ERRO NA FUNÇÃO BFS_multithread.\n");
    }
    else{
        gera_relatorio(g, resultados, &inicio, &fim);
        free(resultados);
    }
    
    // Liberação da memória alocada para o grafo
    grafo_destruir(g);

    return 0;
}
