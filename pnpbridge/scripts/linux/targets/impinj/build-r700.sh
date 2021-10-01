SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
echo SCRIPT_DIR: ${SCRIPT_DIR}

while getopts "bdu" flag
do 
    case "${flag}" in 
        b) noBaseBuild="TRUE";;
        d) noDockerBuild="TRUE";;
        u) noUpgxBuild="TRUE";;
    esac
done

if [ "${noBaseBuild}" != "TRUE" ]; then
    echo Building Binary....
    ${SCRIPT_DIR}/../../build.sh --toolchain-file ${SCRIPT_DIR}/../../../../src/adapters/src/impinj_reader_r700/support_files/toolchainfile.cmake
    cd ${SCRIPT_DIR}/../../../../src/adapters/src/impinj_reader_r700/cap
    make clean
    make
fi

if [ "${noDockerBuild}" != "TRUE" ]; then
    echo Building Docker Image...
    cd ${SCRIPT_DIR}/../../../../../pnpbridge/src/adapters/src/impinj_reader_r700/docker
    ./build-docker-image.sh
    cd ${SCRIPT_DIR}
fi

if [ "${noUpgxBuild}" != "TRUE" ]; then
    echo Building UPGX...
    cd ${SCRIPT_DIR}/../../../../../pnpbridge/src/adapters/src/impinj_reader_r700/support_files
    ./build_upgx_customconfig.sh
fi