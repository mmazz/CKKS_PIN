#!/usr/bin/env python3
"""
Script para demanglear nombres de funciones en CSV generado por OpenFHE Pin Tool
Uso: python demangle_csv.py <ruta_binario> <csv_entrada> <csv_salida>
"""

import subprocess
import sys
import csv
import os
from typing import Dict, Set

def extract_symbols_from_binary(binary_path: str) -> Dict[str, str]:
    """
    Extrae símbolos del binario y crea mapa mangled -> demangled
    """
    print(f"[INFO] Extrayendo símbolos de {binary_path}...")

    if not os.path.exists(binary_path):
        print(f"[ERROR] El binario {binary_path} no existe")
        sys.exit(1)

    # Obtener símbolos con nm
    try:
        proc = subprocess.run(["nm", "-C", "--demangle", binary_path],
                            capture_output=True, text=True, check=True)
        nm_lines = proc.stdout.splitlines()
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] nm falló: {e}")
        sys.exit(1)
    except FileNotFoundError:
        print("[ERROR] 'nm' no encontrado. Instala binutils.")
        sys.exit(1)

    # También obtener símbolos sin demangle para mapeo
    try:
        proc_mangled = subprocess.run(["nm", binary_path],
                                    capture_output=True, text=True, check=True)
        nm_mangled_lines = proc_mangled.stdout.splitlines()
    except subprocess.CalledProcessError:
        nm_mangled_lines = []

    symbol_map = {}

    # Procesar símbolos demangled
    demangled_symbols = {}
    for line in nm_lines:
        parts = line.strip().split()
        if len(parts) >= 3:
            symbol_name = ' '.join(parts[2:])  # Nombre puede tener espacios
            demangled_symbols[parts[2]] = symbol_name

    # Procesar símbolos mangled y crear mapeo
    for line in nm_mangled_lines:
        parts = line.strip().split()
        if len(parts) >= 3:
            mangled_name = parts[2]
            if mangled_name in demangled_symbols:
                symbol_map[mangled_name] = demangled_symbols[mangled_name]
            else:
                # Intentar demangle manual con c++filt
                try:
                    demangle_proc = subprocess.run(["c++filt", mangled_name],
                                                 capture_output=True, text=True)
                    demangled = demangle_proc.stdout.strip()
                    if demangled != mangled_name:  # Se pudo demanglear
                        symbol_map[mangled_name] = demangled
                    else:
                        symbol_map[mangled_name] = mangled_name
                except:
                    symbol_map[mangled_name] = mangled_name

    print(f"[INFO] Extraídos {len(symbol_map)} símbolos")
    return symbol_map

def demangle_function_name(func_name: str, symbol_map: Dict[str, str]) -> str:
    """
    Demanglea un nombre de función usando el mapa de símbolos
    """
    if not func_name or func_name in ["", "UNKNOWN"]:
        return func_name

    # Buscar coincidencia exacta primero
    if func_name in symbol_map:
        return symbol_map[func_name]

    # Buscar coincidencia parcial (para casos donde el nombre está truncado)
    for mangled, demangled in symbol_map.items():
        if func_name in mangled or mangled.endswith(func_name):
            return demangled

    # Si no se encuentra, intentar demangle directo
    try:
        proc = subprocess.run(["c++filt", func_name],
                            capture_output=True, text=True)
        result = proc.stdout.strip()
        return result if result != func_name else func_name
    except:
        return func_name

def clean_function_name(func_name: str) -> str:
    """
    Limpia nombres de función demangled para mejor legibilidad
    """
    if not func_name:
        return func_name

    # Remover espacios extras
    func_name = ' '.join(func_name.split())

    # Simplificar templates muy largos (opcional)
    if len(func_name) > 200:
        # Encontrar el nombre base de la función
        if '(' in func_name:
            base_part = func_name.split('(')[0]
            if '::' in base_part:
                # Tomar el último componente del namespace
                base_part = base_part.split('::')[-1]
            return f"{base_part}(...)"

    return func_name

def process_csv(csv_input: str, csv_output: str,
                symbol_map: Dict[str, str]):
    """
    Procesa el CSV de entrada y genera uno con nombres demangled,
    detectando automáticamente cuántas columnas “padre” hay.
    """
    print(f"[INFO] Procesando {csv_input} -> {csv_output}")
    if not os.path.exists(csv_input):
        print(f"[ERROR] El archivo CSV {csv_input} no existe")
        sys.exit(1)

    with open(csv_input, 'r', encoding='utf-8') as fin, \
         open(csv_output, 'w', encoding='utf-8', newline='') as fout:

        reader = csv.reader(fin)
        writer = csv.writer(fout)

        # --- Leer y reescribir header (sin validar longitudes) ---
        header = next(reader, None)
        if not header:
            print("[ERROR] CSV vacío")
            return
        writer.writerow(header)

        # --- Inferir número de “padres” según columnas restantes ---
        # Asumimos: [ tipo_instr, conteo, func_actual, padre_1, ..., padre_N ]
        if len(header) < 4:
            print(f"[ERROR] Header demasiado corto ({len(header)} cols).")
            return
        num_parents = len(header) - 3
        print(f"[INFO] Detectado num_parents = {num_parents}")

        processed = demangled = 0

        for row_num, row in enumerate(reader, 1):
            if len(row) < 3 + num_parents:
                print(f"[WARNING] Fila {row_num} tiene {len(row)} columnas, "
                      f"esperadas ≥ {3 + num_parents}: {row}")
                continue

            # Extraer campos por posición
            tipo_instr  = row[0]
            conteo      = row[1]
            func_actual = row[2]
            padres      = row[3:3+num_parents]

            # Demangle & clean sobre cada uno
            func_actual_clean = clean_function_name(
                demangle_function_name(func_actual, symbol_map)
            )
            padres_clean = []
            for p in padres:
                padres_clean.append(
                    clean_function_name(
                        demangle_function_name(p, symbol_map)
                    ) if p else ""
                )

            if func_actual_clean != func_actual:
                demangled += 1

            # Escribo en el mismo orden que el header original
            writer.writerow(
                [tipo_instr, conteo, func_actual_clean]
                + padres_clean
            )

            processed += 1
            if processed % 1000 == 0:
                print(f"[INFO] Procesadas {processed} filas...")

    print(f"[INFO] Procesamiento completado:")
    print(f"  - Filas procesadas: {processed}")
    print(f"  - Funciones demangled: {demangled}")
    print(f"  - Archivo generado: {csv_output}")


def main():
    if len(sys.argv) != 5:
        print("Uso: python demangle_csv.py <ruta_binario> <csv_entrada> "
              "<csv_salida> <num_parents>")
        print("Ejemplo:")
        print("  python demangle_csv.py ./mi_programa "
              "input.csv output.csv 3")
        sys.exit(1)

    binary_path = sys.argv[1]
    csv_input    = sys.argv[2]
    csv_output   = sys.argv[3]
    num_parents  = int(sys.argv[4])

    print("=" * 60)
    print("OpenFHE CSV Function Name Demangler")
    print("=" * 60)

    # Extraer símbolos del binario
    symbol_map = extract_symbols_from_binary(binary_path)

    # Procesar CSV con el número dinámico de padres
    process_csv(csv_input, csv_output, symbol_map)

    print("\n[SUCCESS] ¡Demangle completado exitosamente!")

if __name__ == '__main__':
    main()
