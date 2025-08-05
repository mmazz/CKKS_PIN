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
6. Idea:
Pin no recorre todo el binario de entrada de una sola vez, sino que lo hace imagen por imagen (IMG) y por demanda, a medida que el binario (y sus librerías) se van cargando.
Pero dentro de cada imagen, sí, Pin:
    - Recorre rutina por rutina (RTN)
    - Dentro de cada rutina, instrucción por instrucción (INS)
    - Y llama a tus callbacks registrados si corresponde

## Profundizando

Que hace PIN con las rutinas?
Cuando Pin está cargando el binario y encuentra una rutina (RTN) —es decir, una función definida en el ELF:

    1.Llama a tu InstrumentRoutine(rtn, v)

    2. Vos accedés al nombre: RTN_Name(rtn)
    Devuelve el nombre mangleado (si es C++ y no usás extern "C"), tal como aparece en los símbolos.

    3. Luego: RTN_Open(rtn)
    Esto abre la rutina para instrumentarla.
        - Internamente habilita el acceso a sus instrucciones (INS)
        - Sin este Open(), no podés recorrer ni modificar la rutina

    4. Insertás hooks:
       - RTN_InsertCall(... IPOINT_BEFORE ...): se ejecuta antes de entrar a la función
       - RTN_InsertCall(... IPOINT_AFTER ...): se ejecuta cuando la función retorna
       - Ojo que IPOINT_AFTER solo funciona en funciones que tienen return explícito. Si la función usa exit() o throw, puede no llamar AFUNPTR.

    5. RTN_Close(rtn)
        - Finaliza la edición de la rutina.
        - Internamente aplica los cambios (inserciones de tus hooks)
        - Te protege contra corrupción de estado si editás múltiples rutinas simultáneamente
        - Obligatorio: si abriste una rutina con RTN_Open(), tenés que cerrarla
    6. Cuando registro el callback puedo ademas del nombre del callback pasarle
       algun parametro generico (void*), en general uso nullptr. Ese paremtro es
        el que recibe el callback aparte de RTN rtn: que es el objeto que representa la rutina (Pin te lo da).

En instruciones, no hay funciones de open y close ya que son unidades atomicas.

- Extra: Muchas veces podemos usar c11 e insertar una funcion lambda
directamente.
Esto seria pasarle a AFUNPTR esto:
+[]() {
    ...
}
1. No toma argumentos: []()
2. Tiene cuerpo: { measuring = TRUE; ... }
3. Se convierte en puntero a función con +[] (te explico por qué)
4. Se pasa como AFUNPTR(...) para insertarla en un InsertCall de Pin

Sobre 3) Este es el truco: el operador + delante de una lambda fuerza la conversión de la lambda a puntero a función tradicional (tipo void (*)()), pero solo si la lambda no captura variables (lo cual es el caso acá porque el capture list está vacío: []).

Sin el +, la lambda es un objeto tipo class (con operator()), y no sería convertible directamente a AFUNPTR, que espera un void (*)().
## Problemas de simbolos

