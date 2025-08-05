# Entendiendo PIN

Que es una pintool? Una Pin Tool actúa como un editor que hace una pasada por el binario y dice:
"A cada función/instrucción le voy a insertar estos análisis",
y luego el programa corre con esos agregados

Siempre tengo un main en una tool.

1. Esta inicia llamando a PIN_InitSymbols().
Pin escanea el ELF del binario que se va a instrumentar y:
    - Levanta las secciones de símbolos (.symtab, .dynsym)
    - Levanta opcionalmente .debug_* si está compilado con -g
    - Asocia direcciones de memoria con nombres de funciones
    - Te permite usar funciones como:
        - IMG_RtnHead(), RTN_FindByName()
        - RTN_Name(), RTN_Address()
        - INS_File(), INS_Line() (si hay debug info)
Sin esto funciones como RTN_FindByName() o IMG_FindRoutineByName() no funcionarian bien si no se llama a PIN_InitSymbols(), y se obtendrian RTN_Invalid()

Que es la informacion de simbolos?
- Simplemente tablas que que mapean direcciones de memoria a nombres de
funciones, variables, etc.
- Tambien guarda la data de que funciones tiene cada archivo.
- Cuando compilamos con -g tenemos la infromacion DWARF, que no es un acronimo.
- DWARF contiene estructuras muy detalladas sobre:
    - Funciones y variables (nombre, tipo, dirección, visibilidad)
    - Tipos definidos por el usuario (structs, enums, typedefs)
    - Árbol de scopes (funciones anidadas, clases, etc.)
    - Información de línea (line number program)
    - Rutas a los archivos fuente
    - Offsets de pila, registros usados para cada variable
    - Parámetros de funciones

Tipos de símbolos:
    - Símbolos de funciones: main, malloc, my_function, etc.
    - Símbolos de variables globales
    - Símbolos de secciones (.text, .data, etc.)
- Toda esta info va al binario, en las diferentes secciones del elf
    - seccion: .debug_info y .debug_line: Información DWARF: nombres, líneas, archivos fuente, tipos (compilado con -g, hay otras secciones mas de dwarf)
    - seccion: .symtab Tabla de símbolos completa (funciones, variables, etc.)
    (siempre a menos que compilemos con -s )
    - seccion: .strtab 	Nombres de los símbolos (referenciados por .symtab)
    (esto esta siempre)
    - seccion: .dynsym, si hay links dinamicos

2. Sigue con PIN_Init(): Esta función inicializa el runtime de Intel Pin, y parsea los argumentos de la línea de comandos que le pasás a tu herramienta.
- Su rol principal:
    - Procesa KNOBs (los flags configurables de Pin)
    - Inicializa estructuras internas de Pin
    - Verifica que la tool esté bien configurada
    - Prepara el entorno para la instrumentación

3. Funciones: PIN organiza sus funciones como una estructura de grupos.
Program
    - Image (IMG) ← un ejecutable o .so
        - Routine (RTN) ← una función
            - Instruction (INS) ← una instrucción máquina

Algunas funciones por estructura:
- IMG – Imagen (binario cargado)
    - IMG_StartAddress(img) / IMG_HighAddress(img) → rango de direcciones del binario
    - IMG_IsMainExecutable(img) → si es el binario principal o una librería
    - IMG_Valid(img) → verifica si es válido

- RTN – Rutina (función)
    - RTN_Address(rtn) → dirección de entrada
    - RTN_Open(rtn) / RTN_Close(rtn) → permite iterar sobre instrucciones dentro del RTN
    - RTN_Valid(rtn) → verifica si es válida
    - RTN_FindByName(img, "foo") → busca una rutina por nombre dentro de un IMG

- INS – Instrucción: Cada RTN contiene instrucciones (INS), que podés instrumentar una por una.
    - INS_Address(ins) → dirección de la instrucción
    - INS_Disassemble(ins) → instrucción como string (mov eax, ebx)
    - INS_IsCall(ins) / INS_IsRet(ins) / INS_IsBranch(ins) → tipo de instrucción
    - INS_File(ins) / INS_Line(ins) → archivo fuente y línea (si hay DWARF y usás -g)
4. Callbacks
Para que pin pueda realizar cosas a nuestra imagen, funciones, instruciones o lo
que sea, le debemos pasar callbacks. Esto se hace mediante estas funciones:
    - RTN_AddInstrumentFunction: Registra una función callback para instrumentar funciones (routines)
    - INS_AddInstrumentFunction: Registra una función callback para instrumentar instrucciones individuales
    - PIN_AddFiniFunction: Registra una función que se llamará cuando el programa termine

5. Ejecucion: Pin se inicial con PIN_StartProgram(), esto hace:
    - Pin carga e instrumenta el programa:
             a) Mapea las imágenes (ELFs)
             b) Llama a ImageLoad callbacks
             c) Ejecuta funciones de instrumentación (RTN/INS)
             d) Inyecta hooks y trampas en el código
    - Empieza la ejecución real del programa instrumentado
    - Durante la ejecución: Se ejecutan tus análisis (InsertCallbacks)
    - Al finalizar: Llama a tus FiniFunctions (ej: Finish())

## Problemas de simbolos

