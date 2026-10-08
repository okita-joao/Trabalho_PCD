
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

# ----------------------------------------
# 1. LER OS DADOS
# ----------------------------------------

ARQUIVO = "Trabalho_PCD_com_dados_consolidados.xlsx"
PASTA_SAIDA = Path("graficos")
PASTA_SAIDA.mkdir(exist_ok=True)

df = pd.read_excel(
    ARQUIVO,
    sheet_name="Dados Consolidados",
    engine="openpyxl"
)

# Garantir que os dados sejam numéricos
colunas_numericas = [
    "Vértices", "Densidade", "Threads",
    "Tempo (s)", "Speedup", "Eficiência"
]

for coluna in colunas_numericas:
    df[coluna] = pd.to_numeric(
        df[coluna], errors="coerce"
    )

# Configurações do experimento
implementacoes = [
    "OpenMP EM",
    "OpenMP",
    "Pthreads EM",
    "Pthreads"
]

densidades = [0.2, 0.4, 0.6, 0.8]
threads = [1, 2, 4, 8, 16]

# ----------------------------------------
# 2. FUNÇÃO PARA GERAR OS GRÁFICOS
# ----------------------------------------

def gerar_grafico(metrica, titulo, eixo_y, nome_arquivo):
    # Por enquanto, analisamos os maiores grafos
    dados = df[df["Vértices"] == 5000]

    # Figura com quatro subgráficos (2 x 2)
    fig, axes = plt.subplots(
        2, 2, figsize=(11, 7),
        sharex=True
    )

    for ax, densidade in zip(axes.flat, densidades):

        dados_densidade = dados[
            dados["Densidade"].round(2) == densidade
        ]

        for implementacao in implementacoes:

            dados_impl = dados_densidade[
                dados_densidade["Implementação"]
                == implementacao
            ].sort_values("Threads")

            ax.plot(
                dados_impl["Threads"],
                dados_impl[metrica],
                marker="o",
                linewidth=1.8,
                markersize=5,
                label=implementacao
            )

        ax.set_title(
            f"Densidade: {densidade:.0%}"
        )
        ax.set_xticks(threads)
        ax.set_xlabel("Número de threads")
        ax.set_ylabel(eixo_y)
        ax.grid(True, alpha=0.3)

    # Legenda única para toda a figura
    handles, labels = axes.flat[0].get_legend_handles_labels()

    fig.legend(
        handles, labels,
        loc="lower center",
        ncol=4,
        bbox_to_anchor=(0.5, 0.01)
    )

    fig.suptitle(
        titulo + " — 5.000 vértices",
        fontsize=14
    )

    fig.tight_layout(rect=[0, 0.08, 1, 0.94])

    # PDF para o artigo
    fig.savefig(
        PASTA_SAIDA / f"{nome_arquivo}.pdf",
        bbox_inches="tight"
    )

    # PNG para visualização
    fig.savefig(
        PASTA_SAIDA / f"{nome_arquivo}.png",
        dpi=300,
        bbox_inches="tight"
    )

    plt.close(fig)
    print(f"Gráfico gerado: {nome_arquivo}")


# ----------------------------------------
# 3. GERAR OS TRÊS GRÁFICOS
# ----------------------------------------

gerar_grafico(
    metrica="Speedup",
    titulo="Speedup por número de threads",
    eixo_y="Speedup",
    nome_arquivo="01_speedup"
)

gerar_grafico(
    metrica="Tempo (s)",
    titulo="Tempo de execução por número de threads",
    eixo_y="Tempo (s)",
    nome_arquivo="02_tempo"
)

# Recalcula a eficiência a partir do speedup
df["Eficiência"] = df["Speedup"] / df["Threads"]

gerar_grafico(
    metrica="Eficiência",
    titulo="Eficiência por número de threads",
    eixo_y="Eficiência",
    nome_arquivo="03_eficiencia"
)

print("Todos os gráficos foram gerados!")


# ----------------------------------------
# 4. INFLUÊNCIA DO TAMANHO DO GRAFO
# ----------------------------------------

def grafico_tamanho_grafo():
    densidade_fixa = 0.4
    tamanhos = [100, 500, 1000, 5000]

    fig, axes = plt.subplots(
        2, 2, figsize=(11, 7),
        sharex=True
    )

    dados = df[
        df["Densidade"].round(2) == densidade_fixa
    ]

    for ax, implementacao in zip(
        axes.flat, implementacoes
    ):
        dados_impl = dados[
            dados["Implementação"] == implementacao
        ]

        for tamanho in tamanhos:
            dados_tamanho = dados_impl[
                dados_impl["Vértices"] == tamanho
            ].sort_values("Threads")

            ax.plot(
                dados_tamanho["Threads"],
                dados_tamanho["Speedup"],
                marker="o",
                linewidth=1.8,
                markersize=5,
                label=f"{tamanho} vértices"
            )

        ax.set_title(implementacao)
        ax.set_xticks(threads)
        ax.set_xlabel("Número de threads")
        ax.set_ylabel("Speedup")
        ax.grid(True, alpha=0.3)

    handles, labels = (
        axes.flat[0].get_legend_handles_labels()
    )

    fig.legend(
        handles, labels,
        loc="lower center",
        ncol=4,
        bbox_to_anchor=(0.5, 0.01)
    )

    fig.suptitle(
        "Influência do tamanho do grafo no speedup"
        " — Densidade de 40%",
        fontsize=14
    )

    fig.tight_layout(rect=[0, 0.08, 1, 0.94])

    fig.savefig(
        PASTA_SAIDA / "04_tamanho_grafo.pdf",
        bbox_inches="tight"
    )

    fig.savefig(
        PASTA_SAIDA / "04_tamanho_grafo.png",
        dpi=300,
        bbox_inches="tight"
    )

    plt.close(fig)
    print("Gráfico gerado: 04_tamanho_grafo")


grafico_tamanho_grafo()


# ----------------------------------------
# 5. COMPARAÇÃO COM E SEM EXCLUSÃO MÚTUA
# ----------------------------------------

def grafico_exclusao_mutua():
    dados = df[
        (df["Vértices"] == 5000) &
        (df["Densidade"].round(2) == 0.4) &
        (df["Threads"] == 8)
    ]

    dados = dados.set_index(
        "Implementação"
    ).reindex(implementacoes)

    fig, ax = plt.subplots(figsize=(9, 5))

    barras = ax.bar(
        implementacoes,
        dados["Tempo (s)"]
    )

    ax.bar_label(
        barras,
        fmt="%.4f",
        padding=3
    )

    ax.set_title(
        "Comparação com e sem exclusão mútua"
        " — 5.000 vértices, 40%, 8 threads"
    )

    ax.set_ylabel("Tempo de execução (s)")
    ax.set_xlabel("Implementação")
    ax.grid(axis="y", alpha=0.3)
    ax.set_axisbelow(True)

    fig.tight_layout()

    fig.savefig(
        PASTA_SAIDA / "05_exclusao_mutua.pdf",
        bbox_inches="tight"
    )

    fig.savefig(
        PASTA_SAIDA / "05_exclusao_mutua.png",
        dpi=300,
        bbox_inches="tight"
    )

    plt.close(fig)
    print("Gráfico gerado: 05_exclusao_mutua")


grafico_exclusao_mutua()
