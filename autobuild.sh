#!/bin/bash

set -e

# 如果没有build目录，创建该目录，因为build目录通常不会上传到GitHub上
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..