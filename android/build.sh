#!/bin/bash
echo "this is a garbage, do not use it! "
export LC_ALL=C
export OUT_DIR=out-sandcastle
source build/make/envsetup.sh
lunch sandcastle_arm64-userdebug
make -j36
