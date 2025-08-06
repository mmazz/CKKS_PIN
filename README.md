
# Install

'''
cmake -DCMAKE_PREFIX_PATH=$CURDIR/../openfhe-PRNGControl/install\
      -DBUILD_STATIC=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O3 -fno-pie -no-pie" ../src
make -j16

# dependancys

-wget
-unzip

# PIN

'''
export CKKS_CONFIG_PATH="$HOME/CKKS_PIN"

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

## Idea de experimento:

Cambio un poco la cosa, vamos a atacar las intrucciones aritmeticas de


## Uso de config.conf
