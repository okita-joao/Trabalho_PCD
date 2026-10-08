
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.colors import Normalize
from matplotlib.cm import ScalarMappable

# ========================================
# 1. CONFIGURAÇÕES
# ========================================

ARQUIVO = "Trabalho_PCD_com_dados_consolidados.xlsx"
ABA = "Dados Consolidados"

PASTA_BASE = Path("graficos") / "repositorio"

IMPLEMENTACOES = [
    "OpenMP EM",
    "OpenMP",
    "Pthreads EM",
    "Pthreads"
]

TAMANHOS = [100, 500, 1000, 5000]
DENSIDADES = [20, 40, 60, 80]
THREADS = [1, 2, 4, 8, 16]

METRICAS = {
    "speedup": ("Speedup", "Speedup"),
    "tempo": ("Tempo (s)", "Tempo (s)"),
    "eficiencia": ("Eficiência", "Eficiência"),
    "vertices_por_segundo": (
        "Vértices/s", "Vértices por segundo"
    )
}

# ========================================
# 2. LER E PREPARAR OS DADOS
# ========================================

df = pd.read_excel(
    ARQUIVO,
    sheet_name=ABA,
    engine="openpyxl"
)

# Remove espaços extras dos nomes das colunas
df.columns = df.columns.str.strip()

colunas_numericas = [
    "Vértices", "Densidade", "Threads",
    "Tempo (s)", "Speedup",
    "Eficiência", "Vértices/s"
]

# Converte números que eventualmente estejam
# armazenados como texto, incluindo vírgula decimal.
for coluna in colunas_numericas:
    if coluna not in df.columns:
        raise ValueError(
            f"Coluna não encontrada: {coluna}. "
            f"Colunas disponíveis: {df.columns.tolist()}"
        )

    if df[coluna].dtype == "object":
        valores = (
            df[coluna]
            .astype(str)
            .str.strip()
            .str.replace(",", ".", regex=False)
            .str.replace("%", "", regex=False)
        )
        df[coluna] = pd.to_numeric(
            valores, errors="coerce"
        )
    else:
        df[coluna] = pd.to_numeric(
            df[coluna], errors="coerce"
        )

# Padroniza a densidade para porcentagens inteiras:
# 0.2 -> 20; 20 -> 20
if df["Densidade"].dropna().max() <= 1:
    df["Densidade"] *= 100

df["Densidade"] = df["Densidade"].round().astype("Int64")

# Verificações importantes
chaves = [
    "Implementação", "Vértices",
    "Densidade", "Threads"
]

duplicadas = df.duplicated(subset=chaves)

if duplicadas.any():
    raise ValueError(
        "Existem configurações duplicadas na planilha."
    )

print(f"Registros encontrados: {len(df)}")
print("Implementações:", df["Implementação"].unique())

# ========================================
# 3. FUNÇÕES AUXILIARES
# ========================================

def nome_seguro(nome):
    return (
        nome.lower()
        .replace(" ", "_")
        .replace("/", "_")
    )


def salvar_figura(fig, pasta, nome):
    pasta.mkdir(parents=True, exist_ok=True)

    fig.savefig(
        pasta / f"{nome}.png",
        dpi=300,
        bbox_inches="tight"
    )

    plt.close(fig)

# ========================================
# 4. GRÁFICOS POR IMPLEMENTAÇÃO E TAMANHO
# ========================================

def gerar_graficos_metricas():

    for nome_metrica, (coluna, eixo_y) in METRICAS.items():

        pasta = PASTA_BASE / nome_metrica

        for implementacao in IMPLEMENTACOES:
            for tamanho in TAMANHOS:

                fig, ax = plt.subplots(
                    figsize=(8, 5)
                )

                dados_base = df[
                    (df["Implementação"] == implementacao) &
                    (df["Vértices"] == tamanho)
                ]

                for densidade in DENSIDADES:

                    dados = dados_base[
                        dados_base["Densidade"] == densidade
                    ].sort_values("Threads")

                    if dados.empty:
                        continue

                    ax.plot(
                        dados["Threads"],
                        dados[coluna],
                        marker="o",
                        linewidth=1.8,
                        label=f"{densidade}%"
                    )

                ax.set_title(
                    f"{eixo_y} × Threads\n"
                    f"{implementacao} — {tamanho} vértices"
                )

                ax.set_xlabel("Número de threads")
                ax.set_ylabel(eixo_y)
                ax.set_xticks(THREADS)
                ax.grid(True, alpha=0.3)
                ax.legend(title="Densidade")

                fig.tight_layout()

                nome = (
                    f"{nome_metrica}_"
                    f"{nome_seguro(implementacao)}_"
                    f"{tamanho}_vertices"
                )

                salvar_figura(fig, pasta, nome)

        print(f"Concluído: {nome_metrica}")


# ========================================
# 5. COMPARAÇÃO ENTRE IMPLEMENTAÇÕES
# ========================================

def gerar_comparacoes():

    pasta = PASTA_BASE / "comparacao_implementacoes"

    for tamanho in TAMANHOS:
        for densidade in DENSIDADES:

            fig, ax = plt.subplots(
                figsize=(8, 5)
            )

            dados_base = df[
                (df["Vértices"] == tamanho) &
                (df["Densidade"] == densidade)
            ]

            for implementacao in IMPLEMENTACOES:

                dados = dados_base[
                    dados_base["Implementação"] == implementacao
                ].sort_values("Threads")

                if dados.empty:
                    continue

                ax.plot(
                    dados["Threads"],
                    dados["Speedup"],
                    marker="o",
                    linewidth=1.8,
                    label=implementacao
                )

            ax.set_title(
                "Comparação de speedup\n"
                f"{tamanho} vértices — {densidade}%"
            )

            ax.set_xlabel("Número de threads")
            ax.set_ylabel("Speedup")
            ax.set_xticks(THREADS)
            ax.grid(True, alpha=0.3)
            ax.legend()

            fig.tight_layout()

            nome = (
                f"comparacao_{tamanho}_vertices_"
                f"{densidade}pct"
            )

            salvar_figura(fig, pasta, nome)

    print("Concluído: comparação entre implementações")


# ========================================
# 6. MAPAS DE CALOR DE SPEEDUP
# ========================================

def gerar_mapas_calor():

    pasta = PASTA_BASE / "mapas_de_calor"

    # Escala global: mesma cor = mesmo speedup
    valores = df["Speedup"].dropna()

    normalizacao = Normalize(
        vmin=valores.min(),
        vmax=valores.max()
    )

    for implementacao in IMPLEMENTACOES:
        for tamanho in TAMANHOS:

            dados = df[
                (df["Implementação"] == implementacao) &
                (df["Vértices"] == tamanho)
            ]

            matriz = dados.pivot(
                index="Densidade",
                columns="Threads",
                values="Speedup"
            )

            matriz = matriz.reindex(
                index=DENSIDADES,
                columns=THREADS
            )

            fig, ax = plt.subplots(
                figsize=(8, 5)
            )

            imagem = ax.imshow(
                matriz.to_numpy(dtype=float),
                cmap="viridis",
                norm=normalizacao,
                aspect="auto"
            )

            ax.set_xticks(range(len(THREADS)))
            ax.set_xticklabels(THREADS)

            ax.set_yticks(range(len(DENSIDADES)))
            ax.set_yticklabels(
                [f"{d}%" for d in DENSIDADES]
            )

            ax.set_xlabel("Número de threads")
            ax.set_ylabel("Densidade")

            ax.set_title(
                "Mapa de calor do speedup\n"
                f"{implementacao} — {tamanho} vértices"
            )

            # Mostra os valores dentro das células
            for i in range(len(DENSIDADES)):
                for j in range(len(THREADS)):

                    valor = matriz.iloc[i, j]

                    if pd.notna(valor):
                        cor = (
                            "white"
                            if normalizacao(valor) < 0.45
                            else "black"
                        )

                        ax.text(
                            j, i,
                            f"{valor:.2f}",
                            ha="center",
                            va="center",
                            color=cor,
                            fontsize=9
                        )

            fig.colorbar(
                imagem,
                ax=ax,
                label="Speedup"
            )

            fig.tight_layout()

            nome = (
                f"heatmap_"
                f"{nome_seguro(implementacao)}_"
                f"{tamanho}_vertices"
            )

            salvar_figura(fig, pasta, nome)

    print("Concluído: mapas de calor")


# ========================================
# 7. EXECUÇÃO
# ========================================

gerar_graficos_metricas()
gerar_comparacoes()
gerar_mapas_calor()

print()
print("Todos os gráficos foram gerados!")
print(f"Arquivos disponíveis em: {PASTA_BASE}")
