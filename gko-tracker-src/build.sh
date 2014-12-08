cd "$(dirname "$0")"
ROOT_DIR=$(pwd)
BUILD=${ROOT_DIR}/build
OUTPUT=${ROOT_DIR}/output

source ./build.conf
export CMAKE_INCLUDE_PATH=${include_path}
export CMAKE_LIBRARY_PATH=${library_path}

rm -fr ${OUTPUT}
mkdir -p ${OUTPUT} ${BUILD}
cd ${BUILD} && cmake .. -DCMAKE_INSTALL_PREFIX=${OUTPUT}/ && make && make install && cd ..

mkdir -p output/log
cd output && tar --owner=0 --group=0 --mode=-s --mode=go-w -czvf bbts_tracker.tar.gz bin conf log

