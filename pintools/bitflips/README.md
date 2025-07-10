# compile

make obj-intel64/pintool_bitflip.so TARGET=intel64


# Use

 ../pin/pin -t obj-intel64/pintool_bitflip.so -addr 0x8c4ce0 -bit 0 -- ../../build/bin/test 1 1

