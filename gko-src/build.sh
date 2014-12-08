cd "$(dirname "$0")"
PACKAGE_DIR=$(pwd)

source ./build.conf
export CMAKE_INCLUDE_PATH=${include_path}
export CMAKE_LIBRARY_PATH=${library_path}

BUILD=${PACKAGE_DIR}/build
OUTPUT=${PACKAGE_DIR}/output

rm -fr ${OUTPUT}
mkdir -p ${OUTPUT} ${BUILD}
cd ${BUILD} && cmake .. -DCMAKE_INSTALL_PREFIX=${OUTPUT}/ && make && make install && cd ..

TAR="tar --owner=0 --group=0 --mode=-s -zcpf"
GINGKO_PACKAGE="bin conf lib NOAH"

objcopy --only-keep-debug ${OUTPUT}/bin/gko3 ${OUTPUT}/gko3.debug
objcopy --strip-unneeded ${OUTPUT}/bin/gko3
objcopy --add-gnu-debuglink=${OUTPUT}/gko3.debug ${OUTPUT}/bin/gko3

cd ${OUTPUT} && \
find ./bin -type f -exec md5sum {} \; > md5sums && \
find ./conf -type f -exec md5sum {} \; >> md5sums && \
find ./lib -type f -exec md5sum {} \; >> md5sums && \
find ./NOAH -type f -exec md5sum {} \; >> md5sums && mv md5sums NOAH  

cd ${OUTPUT} && ${TAR} gko.tar.gz ${GINGKO_PACKAGE}
