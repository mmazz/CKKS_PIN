
# Install

'''
./install.sh
'''

# dependancys

- wget
- unzip

# PIN

'''
export CKKS_CONFIG_PATH="$HOME/CKKS_PIN"
'''

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
