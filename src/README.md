cmake -DCMAKE_PREFIX_PATH=$HOME/ckksPin/openfhe-PRNGControl/install/lib/OpenFHE \
      -DBUILD_STATIC=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-g -O3 -fno-pie, -no-pie" ..
