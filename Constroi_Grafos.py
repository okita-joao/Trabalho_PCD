import numpy as np
import math
import csv

# Número de vértices que se deseja que o grafo tenha
n = 5

# Porcentagem com relação ao número de vértices que define o número de arestas do grafo
# Caso deseje que o número de arestas possa ser superior ao número de vértices, então percent_arestas = False
percent_arestas = 0.6

# Variável que define se o grafo será completo ou não
completo = False

# Nome do arquivo no qual a matriz de adjacência do grafo será escrita (Obs.: pode colocar o caminho do arquivo também).
file_name = f"grafo.csv"

matriz_adj = np.full((n, n), 1)

if(not completo):
    if(percent_arestas == False):
        # Criando uma matriz de adjacências n x n simétrica (Grafo Não-Direcionado)
        for i in range(0, n):
            arestas = np.random.choice([0, 1], size = n - i)

            for j in range(i, n):
                x = arestas[j - i]
                matriz_adj[i, j] = x
                matriz_adj[j, i] = x

    elif(type(percent_arestas) == float):
        max_arestas = (n*(n-1))/2
        tam = math.floor(n*percent_arestas)

        rng = np.random.default_rng()

        arestas_geradas = rng.choice(np.arange(0, max_arestas), size=tam, replace=False)
        arestas_geradas = arestas_geradas.astype(np.int8)
        lista = arestas_geradas.tolist()

        cont = 0
        inicio = 0
        fim = 0

        for i in range(n-1):
            fim = inicio + i

            linha = []

            for j in range(inicio, fim + 1):
                A = -1

                if(cont < tam):
                    A = lista[cont]

                if(j == A):
                    linha.append(1)
                    cont += 1
                else:
                    linha.append(0)

            index_linha = i+1
            for j in range(0, i+1):
                matriz_adj[index_linha, j] = linha[j]
                matriz_adj[j, index_linha] = linha[j]

            inicio = fim + 1

    else:
        print(f"Erro na declaração da variável percent_arestas, dessa forma gerou-se um grafo completo {n}x{n}.")

# Prevenção de laços no grafo
for i in range(n):
  matriz_adj[i, i] = 0

# Convertendo os valores da matriz para int de 8bits
matriz_adj = matriz_adj.astype(np.int8)

# Escrevendo o grafo gerado num arquivo .csv
with open(file_name, 'w', newline='') as csvfile:
  grafo_writer = csv.writer(csvfile, delimiter=',')
  grafo_writer.writerows(matriz_adj)
