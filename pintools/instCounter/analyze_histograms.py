#!/usr/bin/env python3
"""
Analizador y graficador para datos de OpenFHE Pin Tool
Permite seleccionar nivel de jerarquía de funciones para análisis
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import textwrap
import sys
import argparse
import os
from typing import Optional, List

# Configurar estilo de matplotlib
plt.style.use('default')
sns.set_palette("husl")

def infer_separator(path: str) -> str:
    """Detecta el separador del CSV"""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            first_line = f.readline()
        return '\t' if '\t' in first_line else ','
    except Exception as e:
        print(f"[ERROR] No se pudo leer {path}: {e}")
        sys.exit(1)

def load_and_validate_csv(csv_path: str) -> pd.DataFrame:
    """Carga y valida el CSV de OpenFHE"""
    if not os.path.exists(csv_path):
        print(f"[ERROR] Archivo {csv_path} no encontrado")
        sys.exit(1)

    sep = infer_separator(csv_path)
    print(f"[INFO] Detectado separador: '{sep}'")

    try:
        df = pd.read_csv(csv_path, sep=sep, skip_blank_lines=True, on_bad_lines='skip')
        print(f"[INFO] Cargadas {len(df)} filas")
        print(f"[INFO] Columnas disponibles: {list(df.columns)}")

        # Validar columnas esperadas
        expected_cols = ['Tipo_Instruccion', 'Funcion_Actual', 'Funcion_Padre_1',
                        'Funcion_Padre_2', 'Funcion_Padre_3', 'Conteo']

        if not all(col in df.columns for col in expected_cols):
            print(f"[ERROR] Columnas esperadas: {expected_cols}")
            print(f"[ERROR] Columnas encontradas: {list(df.columns)}")
            sys.exit(1)

        # Convertir conteo a entero
        df['Conteo'] = pd.to_numeric(df['Conteo'], errors='coerce')
        df = df.dropna(subset=['Conteo'])
        df['Conteo'] = df['Conteo'].astype(int)

        print(f"[INFO] Datos válidos después de limpieza: {len(df)} filas")
        return df

    except Exception as e:
        print(f"[ERROR] Error cargando CSV: {e}")
        sys.exit(1)

def get_function_column(hierarchy_level: int) -> str:
    """Retorna el nombre de la columna según el nivel de jerarquía"""
    hierarchy_map = {
        0: 'Funcion_Actual',
        1: 'Funcion_Padre_1',
        2: 'Funcion_Padre_2',
        3: 'Funcion_Padre_3'
    }
    return hierarchy_map.get(hierarchy_level, 'Funcion_Actual')

def create_function_identifier(row: pd.Series, hierarchy_level: int) -> str:
    """Crea un identificador de función según el nivel de jerarquía"""
    if hierarchy_level == 0:
        return row['Funcion_Actual']

    # Construir jerarquía completa hasta el nivel especificado
    parts = []

    # Agregar padres en orden inverso (del más lejano al más cercano)
    for level in range(min(hierarchy_level, 3), 0, -1):
        col_name = get_function_column(level)
        if col_name in row and row[col_name] and str(row[col_name]).strip():
            parts.append(str(row[col_name]).strip())

    # Agregar función actual
    if row['Funcion_Actual'] and str(row['Funcion_Actual']).strip():
        parts.append(str(row['Funcion_Actual']).strip())

    return " -> ".join(parts) if parts else "UNKNOWN"

def plot_instruction_categories(df: pd.DataFrame, output_prefix: str = "openfhe"):
    """Genera histograma por categorías de instrucciones"""
    print("[INFO] Generando gráfico de categorías de instrucciones...")

    # Agrupar por tipo de instrucción
    cat_counts = df.groupby('Tipo_Instruccion')['Conteo'].sum().sort_values(ascending=False)

    plt.figure(figsize=(12, 8))
    bars = plt.bar(range(len(cat_counts)), cat_counts.values,
                   color=sns.color_palette("husl", len(cat_counts)))

    plt.title('Distribución de Instrucciones por Categoría', fontsize=16, fontweight='bold')
    plt.xlabel('Tipo de Instrucción', fontsize=12)
    plt.ylabel('Número de Instrucciones', fontsize=12)

    # Etiquetas en ángulo
    plt.xticks(range(len(cat_counts)), cat_counts.index, rotation=45, ha='right')

    # Agregar valores en las barras
    for i, (bar, value) in enumerate(zip(bars, cat_counts.values)):
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(cat_counts.values)*0.01,
                f'{value:,}', ha='center', va='bottom', fontsize=10)

    plt.grid(axis='y', alpha=0.3)
    plt.tight_layout()

    filename = f'{output_prefix}_categories.png'
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"[SUCCESS] Guardado: {filename}")
    return cat_counts

def plot_top_functions(df: pd.DataFrame, hierarchy_level: int = 0, top_n: int = 15,
                      output_prefix: str = "openfhe"):
    """Genera histograma de top funciones según nivel de jerarquía"""
    print(f"[INFO] Generando gráfico de top {top_n} funciones (nivel jerarquía: {hierarchy_level})...")

    # Crear identificadores de función según jerarquía
    df_copy = df.copy()
    df_copy['Function_ID'] = df_copy.apply(
        lambda row: create_function_identifier(row, hierarchy_level), axis=1
    )

    # Filtrar funciones vacías o unknown
    df_copy = df_copy[df_copy['Function_ID'] != "UNKNOWN"]
    df_copy = df_copy[df_copy['Function_ID'].str.strip() != ""]

    # Agrupar por función y sumar conteos
    func_counts = df_copy.groupby('Function_ID')['Conteo'].sum().sort_values(ascending=False)
    top_functions = func_counts.head(top_n)

    if len(top_functions) == 0:
        print("[WARNING] No se encontraron funciones válidas")
        return None

    # Preparar nombres para display (wrapping)
    display_names = []
    for name in top_functions.index:
        # Truncar nombres muy largos
        if len(name) > 100:
            name = name[:97] + "..."
        wrapped = "\n".join(textwrap.wrap(name, width=50))
        display_names.append(wrapped)

    # Crear gráfico horizontal
    fig, ax = plt.subplots(figsize=(14, max(8, len(top_functions) * 0.6)))

    bars = ax.barh(range(len(top_functions)), top_functions.values,
                   color=sns.color_palette("viridis", len(top_functions)))

    # Configurar títulos y etiquetas
    hierarchy_labels = ["Función Actual", "Función -> Padre 1", "Función -> Padre 1 -> Padre 2",
                       "Función -> Padre 1 -> Padre 2 -> Padre 3"]
    title = f'Top {top_n} Funciones por Instrucciones\n({hierarchy_labels[min(hierarchy_level, 3)]})'

    ax.set_title(title, fontsize=16, fontweight='bold', pad=20)
    ax.set_xlabel('Número de Instrucciones', fontsize=12)
    ax.set_yticks(range(len(top_functions)))
    ax.set_yticklabels(display_names, fontsize=10)

    # Invertir eje Y (mayor arriba)
    ax.invert_yaxis()

    # Agregar valores en las barras
    for i, (bar, value) in enumerate(zip(bars, top_functions.values)):
        ax.text(bar.get_width() + max(top_functions.values)*0.01, bar.get_y() + bar.get_height()/2,
               f'{value:,}', ha='left', va='center', fontsize=10)

    ax.grid(axis='x', alpha=0.3)

    # Ajustar layout para nombres largos
    plt.subplots_adjust(left=0.4)
    plt.tight_layout()

    filename = f'{output_prefix}_top_functions_level_{hierarchy_level}.png'
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"[SUCCESS] Guardado: {filename}")
    return top_functions

def plot_instruction_heatmap(df: pd.DataFrame, hierarchy_level: int = 0,
                           top_functions: int = 10, output_prefix: str = "openfhe"):
    """Genera heatmap de instrucciones por función"""
    print(f"[INFO] Generando heatmap de instrucciones...")

    # Crear identificadores de función
    df_copy = df.copy()
    df_copy['Function_ID'] = df_copy.apply(
        lambda row: create_function_identifier(row, hierarchy_level), axis=1
    )

    # Filtrar y tomar top funciones
    df_filtered = df_copy[df_copy['Function_ID'] != "UNKNOWN"]
    top_func_names = df_filtered.groupby('Function_ID')['Conteo'].sum().nlargest(top_functions).index
    df_heatmap = df_filtered[df_filtered['Function_ID'].isin(top_func_names)]

    if len(df_heatmap) == 0:
        print("[WARNING] No hay datos suficientes para heatmap")
        return

    # Crear pivot table
    pivot_data = df_heatmap.pivot_table(
        values='Conteo',
        index='Function_ID',
        columns='Tipo_Instruccion',
        aggfunc='sum',
        fill_value=0
    )

    # Truncar nombres de funciones para el heatmap
    pivot_data.index = [name[:60] + "..." if len(name) > 60 else name for name in pivot_data.index]

    plt.figure(figsize=(12, max(8, len(pivot_data) * 0.4)))

    # Crear heatmap
    sns.heatmap(pivot_data, annot=True, fmt='d', cmap='YlOrRd',
                cbar_kws={'label': 'Número de Instrucciones'})

    plt.title(f'Heatmap: Tipos de Instrucciones por Función (Top {top_functions})',
              fontsize=14, fontweight='bold')
    plt.xlabel('Tipo de Instrucción', fontsize=12)
    plt.ylabel('Función', fontsize=12)

    plt.xticks(rotation=45, ha='right')
    plt.yticks(rotation=0)
    plt.tight_layout()

    filename = f'{output_prefix}_heatmap_level_{hierarchy_level}.png'
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"[SUCCESS] Guardado: {filename}")

def print_summary_stats(df: pd.DataFrame, hierarchy_level: int = 0):
    """Imprime estadísticas de resumen"""
    print("\n" + "="*60)
    print("RESUMEN ESTADÍSTICO")
    print("="*60)

    # Estadísticas generales
    total_instructions = df['Conteo'].sum()
    unique_instruction_types = df['Tipo_Instruccion'].nunique()

    df_copy = df.copy()
    df_copy['Function_ID'] = df_copy.apply(
        lambda row: create_function_identifier(row, hierarchy_level), axis=1
    )
    unique_functions = df_copy['Function_ID'].nunique()

    print(f"Total de instrucciones ejecutadas: {total_instructions:,}")
    print(f"Tipos únicos de instrucciones: {unique_instruction_types}")
    print(f"Funciones únicas (nivel {hierarchy_level}): {unique_functions}")

    # Top 5 categorías
    print(f"\nTop 5 categorías de instrucciones:")
    top_categories = df.groupby('Tipo_Instruccion')['Conteo'].sum().nlargest(5)
    for i, (cat, count) in enumerate(top_categories.items(), 1):
        percentage = (count / total_instructions) * 100
        print(f"  {i}. {cat}: {count:,} ({percentage:.1f}%)")

    # Top 5 funciones
    print(f"\nTop 5 funciones (nivel jerarquía {hierarchy_level}):")
    top_funcs = df_copy.groupby('Function_ID')['Conteo'].sum().nlargest(5)
    for i, (func, count) in enumerate(top_funcs.items(), 1):
        percentage = (count / total_instructions) * 100
        func_display = func[:80] + "..." if len(func) > 80 else func
        print(f"  {i}. {func_display}: {count:,} ({percentage:.1f}%)")

def main():
    parser = argparse.ArgumentParser(
        description="Analizador de datos OpenFHE Pin Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos de uso:
  python openfhe_analyzer.py data.csv                    # Análisis básico
  python openfhe_analyzer.py data.csv -l 1               # Con jerarquía padre 1
  python openfhe_analyzer.py data.csv -l 2 -t 20         # Top 20, jerarquía padre 2
  python openfhe_analyzer.py data.csv -l 0 -o results    # Personalizar salida
        """
    )

    parser.add_argument('csv_file', help='Archivo CSV de datos OpenFHE')
    parser.add_argument('-l', '--hierarchy-level', type=int, default=0, choices=[0,1,2,3],
                       help='Nivel de jerarquía (0=actual, 1=+padre1, 2=+padre2, 3=+padre3)')
    parser.add_argument('-t', '--top-n', type=int, default=15,
                       help='Número de top funciones a mostrar (default: 15)')
    parser.add_argument('-o', '--output-prefix', default='openfhe',
                       help='Prefijo para archivos de salida (default: openfhe)')
    parser.add_argument('--no-heatmap', action='store_true',
                       help='Omitir generación de heatmap')
    parser.add_argument('--heatmap-functions', type=int, default=10,
                       help='Número de funciones para heatmap (default: 10)')

    args = parser.parse_args()

    print("="*60)
    print("ANALIZADOR OPENFHE PIN TOOL")
    print("="*60)
    print(f"Archivo: {args.csv_file}")
    print(f"Nivel jerarquía: {args.hierarchy_level}")
    print(f"Top N funciones: {args.top_n}")
    print(f"Prefijo salida: {args.output_prefix}")

    # Cargar datos
    df = load_and_validate_csv(args.csv_file)

    # Generar análisis
    plot_instruction_categories(df, args.output_prefix)
    plot_top_functions(df, args.hierarchy_level, args.top_n, args.output_prefix)

    if not args.no_heatmap:
        plot_instruction_heatmap(df, args.hierarchy_level, args.heatmap_functions, args.output_prefix)

    # Mostrar estadísticas
    print_summary_stats(df, args.hierarchy_level)

    print(f"\n[SUCCESS] Análisis completado. Revisa los archivos {args.output_prefix}_*.png")

if __name__ == '__main__':
    main()
