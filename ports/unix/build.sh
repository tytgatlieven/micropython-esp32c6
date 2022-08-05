export LC_ALL="en_US.UTF-8" 
export LC_CTYPE="en_US.UTF-8" 
export LANG="C.UTF-8" 

gmake \
 CROSS_COMPILE=ntoarmv7- V=1 \
 CFLAGS_EXTRA=" \
    -static -static-libgcc \
    -DMP_ENDIANNESS_LITTLE=1 \
    -Wno-missing-field-initializers -Wno-strict-aliasing \
    -D_DIRENT_HAVE_D_TYPE=0 \
  " \
  MPY_LIB_DIR=../../../../packages/micropython-lib \
  MICROPY_STANDALONE=0 VARIANT=ext MICROPY_PY_BLUETOOTH=0 \
  FROZEN_MANIFEST=../../../../packages/manifest.py \
  PROG=micropython
