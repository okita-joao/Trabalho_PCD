#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <pthread.h>
#include <sys/time.h>

// ===========================================================================//
//                              VARIÁVEIS GLOBAIS                             //
// ===========================================================================//

// Caminho do arquivo contendo a matriz original
char caminho[1024] = "grafo_100x100 (1).csv";

#define NUM_THREADS 2

int vertices_iniciais[NUM_THREADS] = {0, 50};

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
        
        saida->prox = NULL;
    }
    
    return saida;
}


// ===========================================================================//
//          CRIAÇÃO DE ESTRUTURAS AUXILIARES PARA EXPLORAÇÃO NO GRAFO         //
// ===========================================================================//

typedef struct{
    int index_vertice;
    int dist;
    int index_pai;
} Vertice;

// ===========================================================================//
//          CRIAÇÃO DA ESTRUTURA DO GRAFO E SEUS RESPECTIVOS MÉTODOS          //
// ===========================================================================//

typedef struct{
    int num_vertices;
    int **matriz_adj;
} Grafo;

// Função que lê um grafo de um arquivo externo:
Grafo* instancia_grafo(char caminho[1024]){
    Grafo *g = NULL;
    
    FILE *fp = fopen(caminho, "r");
    
    if(!fp)
    printf("Não foi possível abrir o arquivo %s", caminho);
    
    else{
        // Inicializando estruturas auxiliares
        Fila *F = (Fila*)malloc(sizeof(Fila));
        if(F == NULL){
            printf("Erro de alocação de memória da fila!\n");
            return NULL;
        }
        
        F->primeiro = NULL;
        F->ultimo = NULL;
        
        // Criando o grafo
        g = (Grafo*)malloc(sizeof(Grafo));
        g->num_vertices = 0;
        g->matriz_adj = NULL;
        
        int n = 0;
        char buffer[1024];
        
        // Lendo a primeira linha da matriz do arquivo
        fgets(buffer, 1024, fp);
        
        char *valor = strtok(buffer, ", ");
        while(valor != NULL){
            int *valor_int = (int*)malloc(sizeof(int));
            
            if(valor_int == NULL){
                printf("Erro ao alocar memória para um inteiro.\n");
                return 0;
            }
            
            *valor_int = atoi(valor);
            void *vpvalor_int = valor_int;
            
            fila_push(F, vpvalor_int);
            valor = strtok(NULL, ", ");
            n++;
        }
        
        // Atualizando os atributos do grafo
        g->num_vertices = n;
        g->matriz_adj = (int**)malloc(n*sizeof(int*));
        
        if(g->matriz_adj == NULL){
            printf("Erro ao alocar memória para a matriz de adjacências.\n");
            return NULL;
        }
        
        // Instanciando a matriz de adjacência do grafo
        for(int i = 0; i < n; i++){
            g->matriz_adj[i] = (int*)calloc(n, sizeof(int));
            if(g->matriz_adj[i] == NULL){
                printf("Erro ao alocar memória na matriz de adjacências.\n");
                return NULL;
            }
        }
        
        // Preenchendo a primeira linha da matriz de adjacências
        for(int i = 0; i < n; i++){
            Celula *cel = fila_pop(F);
            int x = *(int *)cel->conteudo;
            g->matriz_adj[0][i] = x;
            
            free(cel);
        }
        
        free(F);
        
        // Lendo linhas restantes da matriz do arquivo
        int linha = 1, coluna = 0;
        valor = NULL;
        
        while(fgets(buffer, 1024, fp) && linha < n){
            coluna = 0;
            
            valor = strtok(buffer, ", ");
            
            while(valor != NULL && coluna < n){
                g->matriz_adj[linha][coluna] = atoi(valor);
                valor = strtok(NULL, ", ");
                
                coluna++;
            }
            
            linha++;
        }
        
        fclose(fp);
    }
    
    return g;
}

void print_grafo(Grafo *g){
    printf("Grafo:\n");
    printf("Numero de Vertices = %d\n", g->num_vertices);
    
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
    Vertice ***resultados;
} arg;

void* BFS(void *argumento){
    arg dado = *(arg *)argumento;
    Grafo *g = dado.g;
    int index_thread = dado.index_thread;
    int index_vertice_origem = dado.index_vertice_origem;
    int *pretos = dado.pretos;
    int *cinzas = dado.cinzas;
    Vertice ***resultados = dado.resultados;
    
    // Inicialização dos vértices e estruturas auxiliares
    Vertice **vertices = (Vertice**)malloc(g->num_vertices*sizeof(Vertice*));
    if(vertices == NULL){
        printf("Erro ao alocar memória para o vetor de vértices.\n");
        return NULL;
    }
    
    for(int i = 0; i < g->num_vertices; i++){
        vertices[i] = (Vertice*)malloc(sizeof(Vertice));
        if(vertices[i] == NULL){
            printf("Erro ao alocar memória para um vértice.\n");
            return NULL;
        }
        vertices[i]->dist = -1;
        vertices[i]->index_pai = i;
        vertices[i]->index_vertice = i;
    }
    
    Fila *Q = (Fila*)malloc(sizeof(Fila));
    if(Q == NULL){
        printf("Erro ao alocar memória para a fila de vértices.\n");
        return NULL;
    }
    
    Q->primeiro = NULL;
    Q->ultimo = NULL;
    
    void *p = &index_vertice_origem;
    fila_push(Q, p);
    
    vertices[index_vertice_origem]->dist = 0;
    cinzas[index_vertice_origem] = 1;

    pthread_mutex_lock(&trava_print);

    printf("\nThread %d preparada para iniciar!\n", index_thread);

    pthread_mutex_unlock(&trava_print);

    // Barreira para que todas as threads iniciem ao mesmo tempo
    pthread_barrier_wait(&barreira_inicializacao);
    
    // Exploração do grafo em Largura
    int *pa;
    while(Q->primeiro != NULL && Q->ultimo != NULL){
        Celula *u = fila_pop(Q);
        if(u == NULL){
            printf("Erro ao explorar o grafo pelo BFS.\n");
            return NULL;
        }
        
        int u_index = *(int *)u->conteudo;
        for(int i = 0; i < g->num_vertices; i++){
            if(g->matriz_adj[u_index][i] != 0 && cinzas[i] == 0){
                int *v_index = (int*)malloc(sizeof(int));
                if(v_index == NULL){
                    printf("Erro ao alocar memoria para um inteiro.\n");
                    return NULL;
                }
                *v_index = i;
                
                fila_push(Q, v_index);
                
                cinzas[i] = 1;
                vertices[i]->index_pai = u_index;
                vertices[i]->dist = vertices[u_index]->dist + 1;
            }
        }
        
        pretos[u_index] = 1;
        free(u);
    }
    
    free(Q);

    resultados[index_thread] = vertices;

    pthread_exit(NULL);
}

// ===========================================================================//
//                    IMPLEMENTAÇÃO DAS THREADS VIA PTHREADS                  //
// ===========================================================================//

Vertice*** BFS_multithread(Grafo *g){
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

    Vertice ***resultados = (Vertice***)calloc(NUM_THREADS, sizeof(Vertice**));
    if(resultados == NULL){
        printf("Erro ao alocar memória para o vetor de resultados.\n");
        return NULL;
    }

    // Criando vetor global para cinzas e pretas do processo
    int *pretos = (int*)calloc(g->num_vertices, sizeof(int));
    int *cinzas = (int*)calloc(g->num_vertices, sizeof(int));

    if(pretos == NULL || cinzas == NULL){
        printf("Erro ao alocar memória para o vetor de pretos ou cinzas.\n");
        return NULL;
    }

    // Iniciando a barreira de inicialização e o mutex de prints
    pthread_barrier_init(&barreira_inicializacao, NULL, NUM_THREADS + 1);
    pthread_mutex_init(&trava_print, NULL);

    for(int i = 0; i < NUM_THREADS; i++){
        arg *argumento = (arg*)malloc(sizeof(arg));
        if(argumento == NULL){
            printf("Erro ao alocar memória para o argumento.\n");
            return NULL;
        }

        argumento->g = g;
        argumento->index_thread = i;
        argumento->index_vertice_origem = vertices_iniciais[i];
        argumento->cinzas = cinzas;
        argumento->pretos = pretos;
        argumento->resultados = resultados;

        pthread_create(&threads[i], NULL, BFS, (void*)argumento);
    }

    // 2. Medição do tempo de execução.

    struct timeval inicio, fim;

    gettimeofday(&inicio, NULL);

    pthread_barrier_wait(&barreira_inicializacao);

    printf("Barreira liberada, threads em acao!\n");

    for(int i = 0; i < NUM_THREADS; i++){
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&fim, NULL);

    double tempo_gasto = (fim.tv_sec - inicio.tv_sec) + (fim.tv_usec - inicio.tv_usec) / 1000000.0;

    printf("Tempo gasto: %d\n", tempo_gasto);

    // 3. Limpeza de memória
    free(cinzas);
    free(pretos);
    free(threads);

    pthread_barrier_destroy(&barreira_inicializacao);
    pthread_mutex_destroy(&trava_print);

    // 4. Retorno da função
    return resultados;
}


// ===========================================================================//
//                              PROGRAMA PRINCIPAL                            //
// ===========================================================================//

int main(){
    
    Grafo *g = instancia_grafo(caminho);
    
    // Rodando a Busca em Largura:
    printf("Busca em Largura (BFS):\n\n");
    Vertice ***resultados = BFS_multithread(g);

    if(resultados == NULL){
        printf("ERRO NA FUNÇÃO BFS_multithread.\n");
    }
    else{
        for(int i = 0; i < NUM_THREADS; i++){
            printf("Navegacao feita pela Thread %d:\n", i);
            Vertice **vertices = resultados[i];
            for(int j = 0; j < g->num_vertices; j++){
                int index = vertices[j]->index_vertice;
                int dist = vertices[j]->dist;
                int pai = vertices[j]->index_pai;

                printf("Vertice (%d):\nIndex_pai: %d\nDist:%d\n\n", index, pai, dist);
            }
        }
    }

    for(int i = 0; i < NUM_THREADS; i++){
        free(resultados[i]);
    }
    free(resultados);
    
    return 0;
}
