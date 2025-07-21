# Idea


Por el momento el tool hace lo siguiente:
Defino en mi main dos labels.
Ejecuto lo que quiero de openfhe, por ejemplo hasta el cifrado.
Una vez que tengo lo que quiero atacar guardo en un archivo el puntero al objeto, por ejemplo
el c0 y ademas me guardo el puntero al arreglo donde estan los coeficientes de c0 de uno de los limbs.

Luego de esto uso un label (addr_label). Con esto me aseguro que PIN lea el archivo recien ahora y no antes.
En mi main, ahora hago un loop por cada bit y coeficiente en donde hago lo que quiero, por ejemplo computar, desencriptar, calculo norma2, etc.

Luego al final de la iteracion llamo al siguiente label (sync_marker).
Con esto le digo a PIN que restaure el estado. Basicamente es una copia a mi cifrado original y vuelve a empezar.

## Compile and use


'''
make build-pin-check
make run-pin-check
'''


## function name

In the directory of the cpp bin: Buscamos primero el cambio de formato para poder emular el uso sin NTT

'''
nm --defined-only bitflip_check   | grep ' W '   | c++filt   | grep 'SwitchFormat'
00000000004ce0a0 W lbcrypto::DCRTPolyImpl<bigintdyn::mubintvec<bigintdyn::ubint<unsigned long> > >::SwitchFormat()
'''
Teniendo el nombre buscamos el simbolo sin el demangle

'''
nm --defined-only bitflip_check   | grep ' T '   | c++filt   | grep 'Decrypt('
'''
Agarrando la memoria vuelvo a buscar sin el --demangle haciendo grep a ese valor.

Openfhe: export CKKS_CONFIG_PATH=$HOME/CKKS_PIN
../pin/pin -t obj-intel64/pintool_bitflip_func.so -label addr_file -func Mangle_func_name -coeff 1 -bit 0 -- ../../build/bin/test 1 1

