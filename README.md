
# OpenFHE static

'''
cmake -DCMAKE_INSTALL_PREFIX=$HOME/ckksPin/openfhe-static -DBUILD_STATIC=ON \
      -DBUILD_SHARED=OFF -DCMAKE_BUILD_TYPE=Release -DWITH_OPENMP=OFF \
      -DBUILD_UNITTESTS=OFF -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF ..

make -j16
'''
# PIN

We compile it with g++-11.

In case of arch linux distro or similar roling release distro, install another version
like gcc-11, and then before ejecute makefile export these variables.
'''
export CC=gcc-11
export CXX=g++-11
'''

### Bug

Hay un bug en: components/include/util/range.hpp

en vez de m_base en la linea 102, tiene que ir _base.

## Use of PIN

First we export a env variable for gcc-11

Put the tool in pin/source/tools/SimpleExamples

And inside that directory do:

'''
sudo make obj-intel64/my_tool.so TARGET=intel64
'''



Second we need to find the symbol of the function that we want to tackle.

We use nm for that.
For example if we want to attack the Encrypt function that is implemented using privatekey, we
can do:
'''
nm -C ./test | grep 'Encrypt(' | grep 'PrivateKey'
'''

