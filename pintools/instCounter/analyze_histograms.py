"""
Lee un CSV delimitado por tabulaciones o comas,
y genera dos histogramas:
  - Instrucciones por categoría
  - Top 10 funciones
"""
import pandas as pd
import matplotlib.pyplot as plt
import textwrap
import sys


def infer_separator(path):
    with open(path) as f:
        first = f.readline()
    return '\t' if '\t' in first else ','


def plot_histograms(csv_path):
    sep = infer_separator(csv_path)
    df = pd.read_csv(csv_path, sep=sep, skip_blank_lines=True,
                     on_bad_lines='skip')

    # Asegurar columnas correctas
    df = df[['Function', 'Instruction_Type', 'Count']]
    df['Count'] = df['Count'].astype(int)

    # Histograma por categoría
    cat = df.groupby('Instruction_Type')['Count'].sum().sort_values(ascending=False)
    plt.figure(figsize=(8,5))
    cat.plot(kind='bar', title='Instrucciones por categoría')
    plt.ylabel('Count')
    plt.tight_layout()
    plt.savefig('hist_instructions_by_category.png')
    plt.close()

    # Top 10 funciones (wrap labels)
    top = df.groupby('Function')['Count'].sum().nlargest(10)
    func_names = top.index.tolist()
    wrapped = ["\n".join(textwrap.wrap(name, width=60)) for name in func_names]
    counts = top.values

    fig, ax = plt.subplots(figsize=(14, 10))
    ax.barh(wrapped, counts)
    ax.set_title('Top 10 funciones por número de instrucciones')
    ax.set_xlabel('Count')
    ax.invert_yaxis()
    fig.subplots_adjust(left=0.5)  # más espacio aún para eje Y
    plt.savefig('hist_top10_functions.png')
    plt.close()

    print('Generados: hist_instructions_by_category.png, hist_top10_functions.png')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Uso: python analyze_histograms.py <csv_path>")
        sys.exit(1)
    plot_histograms(sys.argv[1])
def infer_separator(path):
    with open(path) as f:
        first = f.readline()
    return '\t' if '\t' in first else ','


def plot_histograms(csv_path):
    sep = infer_separator(csv_path)
    df = pd.read_csv(csv_path, sep=sep, skip_blank_lines=True,
                     on_bad_lines='skip')

    # Asegurar columnas correctas
    df = df[['Function', 'Instruction_Type', 'Count']]
    df['Count'] = df['Count'].astype(int)

    # Histograma por categoría
    cat = df.groupby('Instruction_Type')['Count'].sum().sort_values(ascending=False)
    plt.figure(figsize=(8,5))
    cat.plot(kind='bar', title='Instrucciones por categoría')
    plt.ylabel('Count')
    plt.tight_layout()
    plt.savefig('hist_instructions_by_category.png')
    plt.close()

    # Top 10 funciones (wrap labels)
    top = df.groupby('Function')['Count'].sum().nlargest(10)
    func_names = top.index.tolist()
    wrapped = ["\n".join(textwrap.wrap(name, width=40)) for name in func_names]
    counts = top.values

    plt.figure(figsize=(12,8))
    plt.barh(wrapped, counts)
    plt.title('Top 10 funciones por número de instrucciones')
    plt.xlabel('Count')
    plt.gca().invert_yaxis()  # Mayor arriba
    plt.subplots_adjust(left=0.4)  # más espacio para eje Y
    plt.tight_layout()
    plt.savefig('hist_top10_functions.png')
    plt.close()

    print('Generados: hist_instructions_by_category.png, hist_top10_functions.png')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Uso: python analyze_histograms.py <csv_path>")
        sys.exit(1)
