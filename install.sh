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
    INSTALL_PATH=$PWD/$OPENFHE_DIR
    cd $OPENFHE_DIR/build
    cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PATH/install -DBUILD_STATIC=ON -DBUILD_SHARED=OFF \
      -DCMAKE_BUILD_TYPE=Debug -DWITH_OPENMP=OFF -DBUILD_UNITTESTS=OFF \
      -DBUILD_BENCHMARKS=OFF -DBUILD_EXTRAS=OFF -DCMAKE_CXX_FLAGS="-g" ..
    make -j16
    sudo make install
    cd ../..
else
    echo "$OPENFHE_DIR/build already exists."
fi

mkdir -p analysis/img
mkdir -p src
mkdir -p build
INSTALL_PATH_SRC=$PWD/$OPENFHE_DIR/install/lib/OpenFHE
echo "Installing on " $INSTALL_PATH_SRC
cd build
cmake -DCMAKE_PREFIX_PATH=$INSTALL_PATH_SRC -DBUILD_STATIC=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-g" ..
make -j16
