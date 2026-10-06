import numpy as np
import csv

def gerar_grafo(n, densidade, arquivo):
    matriz = np.zeros((n, n), dtype=np.int8)

    # Número máximo de arestas
    max_arestas = n * (n - 1) // 2

    # Número desejado de arestas
    num_arestas = int(max_arestas * densidade)

    # ------------------------------------------------
    # 1. Cria uma árvore para garantir conectividade
    # ------------------------------------------------

    rng = np.random.default_rng()

    vertices = np.arange(n)
    rng.shuffle(vertices)

    for i in range(1, n):
        u = vertices[i]
        v = vertices[rng.integers(0, i)]

        matriz[u][v] = 1
        matriz[v][u] = 1

    arestas_atual = n - 1

    # ------------------------------------------------
    # 2. Adiciona arestas aleatórias
    # ------------------------------------------------

    while arestas_atual < num_arestas:

        u = rng.integers(0, n)
        v = rng.integers(0, n)

        # Evita laço
        if u == v:
            continue

        # Evita aresta repetida
        if matriz[u][v] == 1:
            continue

        matriz[u][v] = 1
        matriz[v][u] = 1

        arestas_atual += 1

    # ------------------------------------------------
    # 3. Salva no CSV
    # ------------------------------------------------

    with open(arquivo, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerows(matriz)

    return matriz


# Exemplo
n = 1000
densidade = 0.60

gerar_grafo(
    n,
    densidade,
    "grafo_1000_60.csv"
)
