"""
Este script ejecuta `nm` para listar los símbolos mangleados del binario y luego usa `c++filt` para obtener los nombres demangleados.
Reemplaza en el CSV original los nombres mangleados por los demangleados.
"""
import subprocess
import sys

def demangle_names(binary_path, csv_in, csv_out):
    # Ejecuta nm sin demangle para obtener nombres mangleados
    proc = subprocess.run(["nm", binary_path], capture_output=True, text=True)
    sym_lines = proc.stdout.splitlines()

    # Construye mapa: mangle -> demangle
    demap = {}
    for line in sym_lines:
        parts = line.strip().split()
        if len(parts) >= 3:
            mangle = parts[-1]
            # Demangle con c++filt
            dproc = subprocess.run(["c++filt", mangle], capture_output=True, text=True)
            demap[mangle] = dproc.stdout.strip()

    # Procesa CSV original y reemplaza nombres
    with open(csv_in, 'r') as fin, open(csv_out, 'w') as fout:
        header = fin.readline()
        fout.write(header)
        for line in fin:
            parts = line.strip().split(',')
            if len(parts) != 4:
                continue  # ignora líneas mal formateadas
            func, addr, cat, count = parts
            demangled = demap.get(func, func)
            fout.write(f"{demangled},{addr},{cat},{count}\n")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Uso: python demangle_objdump.py <ruta_binario> <csv_entrada> <csv_salida>")
        sys.exit(1)
    demangle_names(sys.argv[1], sys.argv[2], sys.argv[3])

