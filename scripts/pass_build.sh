#! /usr/bin/sh

# For peroni
# source /project/extra/llvm/9.0.0/enable
nwd=$(pwd)

# Copy the source into the toolchain for build
cd ./src/llvm/$1/

cp ./driver/$2 ../toolchain/catpass/CatPass.cpp
cp ./CMakeLists.txt ../toolchain/catpass/
cp -ar ./src/ ../toolchain/catpass/
cp -ar ./include/ ../toolchain/catpass/

# Build from the toolchain
cd ../toolchain 
./run_me.sh

cd $nwd # Nautilus working directory --- very suspicious
