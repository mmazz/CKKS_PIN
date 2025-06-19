

# PIN

We compile it with g++-11.

In case of arch linux distro or similar roling release distro, install another version
like gcc-11, and then before ejecute makefile export these variables.
'''
export CC=gcc-11
export CXX=g++-11
'''

## Use of PIN

First we export a env variable.
In my machine pin is installed in:
'''
export PIN_ROOT=/opt/pin
'''

'''
g++ -Wall -Werror -O2 -fPIC -std=c++11 -I${PIN_ROOT}/source/include/pin \
    -I${PIN_ROOT}/source/include/pin/gen \
    -c mytool.cpp -o mytool.o
'''


Second we need to find the symbol of the function that we want to tackle.

We use nm for that.
For example if we want to attack the Encrypt function that is implemented using privatekey, we
can do:
'''
nm -C ./test | grep 'Encrypt(' | grep 'PrivateKey'
'''

