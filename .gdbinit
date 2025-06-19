set skip on

# Saltar headers del sistema
skip -gfi /usr/include/*
skip -gfi /usr/local/include/*
skip -gfi **/bits/*.h
skip -gfi **/c++/*

# Saltar funciones de std y plantillas internas
skip function std::*
skip function std::__*
skip function __gnu_cxx::*

# Opción (más agresiva): salta toda función con __ en el nombre
# skip function *__::__*

