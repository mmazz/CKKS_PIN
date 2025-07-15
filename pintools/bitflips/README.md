# compile

make obj-intel64/pintool_bitflip.so TARGET=intel64


# Use

## function name

In the directory of the cpp bin:
'''
nm --defined-only bitflip_check   | grep ' T '   | c++filt   | grep 'Decrypt('
'''

Agarrando la memoria vuelvo a buscar sin el --demangle haciendo grep a ese valor.

Openfhe: export CKKS_CONFIG_PATH=$HOME/CKKS_PIN
../pin/pin -t obj-intel64/pintool_bitflip_func.so -label addr_file -func Mangle_func_name -coeff 1 -bit 0 -- ../../build/bin/test 1 1

