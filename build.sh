###############################################################################
# Copyright 2017 Samsung Electronics All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###############################################################################

#!/bin/bash
echo "Start edge opcua build"

usage() {
    echo -e "${BLUE}Usage:${NO_COLOUR} ./build.sh <option>"
    echo -e "${GREEN}Options:${NO_COLOUR}"
    echo "  --build_mode=[release|debug(DEBUG)](default: release) :  Build opcua library and samples in release or debug mode"
}

process_cmd_args() {
    while [ "$#" -gt 0  ]; do
            case "$1" in
            --build_mode=*)
                build_mode="${1#*=}";
                echo -e "Build mode = $build_mode"
                shift 1;
                ;;
            -h)
                usage; exit 0
                ;;
            --help)
                usage; exit 0
                ;;
            -*)
                echo "unknown option: $1" >&2;
                usage; exit 1
                ;;
            *)
                echo "unknown option: $1" >&2;
                usage; exit 1
                ;;
        esac
    done
}
process_cmd_args "$@"

pip_dir=$(command -v pip)
pip_dir_has_six=$(pip list | grep six)

if [ $pip_dir == "" ]
then
    echo "Please install python-pip"
    echo "## curl -O https://bootstrap.pypa.io/get-pip.py"
    echo "## python get-pip.py"
    echo "Build failed"
    exit 1
elif [ "$pip_dir_has_six" == "" ]
then
    echo "Install python-six..."
    pip install --trusted-host files.pythonhosted.org --trusted-host pypi.org --upgrade --ignore-installed six
else
    echo "Has python-six module..."
fi

if [ "$build_mode" == debug -o "$build_mode" == DEBUG ]
    then
    echo "Build with DEBUG mode"
    scons -c
    scons TARGET_ARCH=linux TEST=1 AUTO_DOWNLOAD_DEP_LIBS=1 DEBUG=1 SUB_QUEUE=1
    if [ $? -ne 0 ]; then
        echo -e "Build failed"
        exit 1
    fi
else
    echo "Build with RELEASE mode"
    scons -c
    scons TARGET_ARCH=linux TEST=1 AUTO_DOWNLOAD_DEP_LIBS=1 SUB_QUEUE=1
    if [ $? -ne 0 ]; then
        echo -e "Build failed"
        exit 1
    fi
fi

echo "End of edge opcua build"
