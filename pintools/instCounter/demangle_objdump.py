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

def process_csv(csv_input: str, csv_output: str, symbol_map: Dict[str, str]):
    """
    Procesa el CSV de entrada y genera uno con nombres demangled
    """
    print(f"[INFO] Procesando {csv_input} -> {csv_output}")

    if not os.path.exists(csv_input):
        print(f"[ERROR] El archivo CSV {csv_input} no existe")
        sys.exit(1)

    processed_count = 0
    demangled_count = 0

    try:
        with open(csv_input, 'r', encoding='utf-8') as fin, \
             open(csv_output, 'w', encoding='utf-8', newline='') as fout:

            reader = csv.reader(fin)
            writer = csv.writer(fout)

            # Procesar header
            try:
                header = next(reader)
                writer.writerow(header)
                print(f"[INFO] Header: {header}")
            except StopIteration:
                print("[ERROR] CSV vacío")
                return

            # Procesar filas de datos
            for row_num, row in enumerate(reader, 1):
                if len(row) != 6:  # Esperamos 6 columnas según el nuevo formato
                    print(f"[WARNING] Fila {row_num} tiene {len(row)} columnas, esperadas 6: {row}")
                    continue

                tipo_instr, func_actual, padre_1, padre_2, padre_3, conteo = row

                # Demanglear cada función en la jerarquía
                func_actual_clean = clean_function_name(
                    demangle_function_name(func_actual, symbol_map)
                )
                padre_1_clean = clean_function_name(
                    demangle_function_name(padre_1, symbol_map)
                ) if padre_1 else ""
                padre_2_clean = clean_function_name(
                    demangle_function_name(padre_2, symbol_map)
                ) if padre_2 else ""
                padre_3_clean = clean_function_name(
                    demangle_function_name(padre_3, symbol_map)
                ) if padre_3 else ""

                # Contar cuántos se demangled
                if func_actual_clean != func_actual:
                    demangled_count += 1

                # Escribir fila procesada
                writer.writerow([
                    tipo_instr,
                    func_actual_clean,
                    padre_1_clean,
                    padre_2_clean,
                    padre_3_clean,
                    conteo
                ])

                processed_count += 1

                # Progreso cada 1000 filas
                if processed_count % 1000 == 0:
                    print(f"[INFO] Procesadas {processed_count} filas...")

    except Exception as e:
        print(f"[ERROR] Error procesando CSV: {e}")
        sys.exit(1)

    print(f"[INFO] Procesamiento completado:")
    print(f"  - Filas procesadas: {processed_count}")
    print(f"  - Funciones demangled: {demangled_count}")
    print(f"  - Archivo generado: {csv_output}")

def main():
    if len(sys.argv) != 4:
        print("Uso: python demangle_csv.py <ruta_binario> <csv_entrada> <csv_salida>")
        print("\nEjemplo:")
        print("  python demangle_csv.py ./mi_programa_openfhe openfhe_ckks_counts.csv openfhe_demangled.csv")
        sys.exit(1)

    binary_path = sys.argv[1]
    csv_input = sys.argv[2]
    csv_output = sys.argv[3]

    print("=" * 60)
    print("OpenFHE CSV Function Name Demangler")
    print("=" * 60)

    # Extraer símbolos del binario
    symbol_map = extract_symbols_from_binary(binary_path)

    # Procesar CSV
    process_csv(csv_input, csv_output, symbol_map)

    print("\n[SUCCESS] ¡Demangle completado exitosamente!")

if __name__ == '__main__':
    main()
