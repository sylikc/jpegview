#!/usr/bin/env bash

# --forward is optional, and added to make scripting calls to this easier

# apng support
echo "--- Patching libpng with apng support ---"
patch -d libpng -p1 --forward < libpng-apng/libpng-1.6.39-apng.patch
# error check, fail and return error if failed
if [ $? -ne 0 ]; then
	echo apng patch failed
	exit 1
fi


# 64-bit support
echo "--- Patching libpng with 64-bit support ---"
patch -d libpng -p1 --forward < libpng-x64.patch
if [ $? -ne 0 ]; then
	echo x64 patch failed
	exit 1
fi
