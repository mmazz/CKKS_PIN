#!/bin/bash

OPENFHE_DIR="openfhe-PRNGControl"

echo "Initializing"
if [[ ! -d "$OPENFHE_DIR" ]]; then
    echo "Cloning"
    git clone --depth=1 git@github.com:mmazz/openfhe-PRNGControl.git
else
    echo "$OPENFHE_DIR already exists."
fi
echo ""
if [[ ! -d "$OPENFHE_DIR/build" ]]; then
    echo "Building"
    cd $OPENFHE_DIR/third-party/
    git clone https://github.com/openfheorg/cereal
    cd ../..
    mkdir -p $OPENFHE_DIR/build
    cd $OPENFHE_DIR/build
    cmake -DCMAKE_INSTALL_PREFIX=$PWD/$OPENFHE_DIR/install -DBUILD_STATIC=ON -DBUILD_SHARED=OFF \
      -DCMAKE_BUILD_TYPE=Debug -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF -DCMAKE_CXX_FLAGS="-g" ..
    make -j16
    sudo make install
    cd ../..
else
    echo "$OPENFHE_DIR/build already exists."
fi

mkdir -p analysis/img
mkdir src
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=$PWD/$OPENFHE_DIR/install/lib/OpenFHE -DBUILD_STATIC=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-g" ..
make -j16
