#!/bin/bash
echo "Need to proceed..."
export LC_ALL=C
export OUT_DIR=out-sandcastle
source build/make/envsetup.sh
lunch sandcastle_arm64-userdebug
make -j36
