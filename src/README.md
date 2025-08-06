cmake -DCMAKE_PREFIX_PATH=$HOME/CKKS_PIN/openfhe-PRNGControl/install/lib/OpenFHE \
      -DBUILD_STATIC=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O3 -fno-pie -no-pie" ../src
