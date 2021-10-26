#!/usr/bin/env bash
#
# Find apps and build apps for example_test, custom_test, and unit_test
#
# Runs as part of CI process.
#

# -----------------------------------------------------------------------------
# Safety settings (see https://gist.github.com/ilg-ul/383869cbb01f61a51c4d).

if [[ -n ${DEBUG_SHELL} ]]; then
  set -x # Activate the expand mode if DEBUG is anything but empty.
fi

if [ -z ${CI_NODE_TOTAL} ]; then
  CI_NODE_TOTAL=1
  echo "Assuming CI_NODE_TOTAL=${CI_NODE_TOTAL}"
fi

if [ -z ${CI_NODE_INDEX} ]; then
  # Gitlab uses a 1-based index
  CI_NODE_INDEX=1
  echo "Assuming CI_NODE_INDEX=${CI_NODE_INDEX}"
fi

set -o errexit  # Exit if command failed.
set -o pipefail # Exit if pipe failed.
set -o nounset  # Exit if variable not set.

# -----------------------------------------------------------------------------

die() {
  echo "${1:-"Unknown Error"}" 1>&2
  exit 1
}


[ -d ${BUILD_PATH} ] || mkdir -p ${BUILD_PATH}
[ -d ${LOG_PATH} ] || mkdir -p ${LOG_PATH}
[ -f ${SIZE_INFO_LOCATION} ] && rm ${SIZE_INFO_LOCATION}

echo "${BUILD_PATH} ${LOG_PATH} ${SIZE_INFO_LOCATION}"

export REALPATH=realpath
if [ "$(uname -s)" = "Darwin" ]; then
  export REALPATH=grealpath
fi

# Convert LOG_PATH and BUILD_PATH to relative, to make the json file less verbose.
# BUILD_PATH=$(${REALPATH} --relative-to ${IDF_PATH} ${BUILD_PATH})
# LOG_PATH=$(${REALPATH} --relative-to ${IDF_PATH} ${LOG_PATH})

# ALL_BUILD_LIST_JSON="${BUILD_PATH}/list.json"
# JOB_BUILD_LIST_JSON="${BUILD_PATH}/list_job_${CI_NODE_INDEX}.json"

# -----------------------------------------------------------------------------
# common variables, will specify special cases later
# WORK_DIR="--work-dir ${BUILD_PATH}/@f/@w/@t"
# BUILD_DIR="build"
# BUILD_LOG="${LOG_PATH}/@f_@w_@t.txt"
# CONFIG="--config sdkconfig.ci=default
#         --config sdkconfig.ci.*=
#         --config =default"

export EXTRA_CFLAGS="${PEDANTIC_CFLAGS}"
export EXTRA_CXXFLAGS="${PEDANTIC_CXXFLAGS}"

# --config rules above explained:
# 1. If sdkconfig.ci exists, use it build the example with configuration name "default"
# 2. If sdkconfig.ci.* exists, use it to build the "*" configuration
# 3. If none of the above exist, build the default configuration under the name "default"
# --work-dir and --build-log above uses "placeholders" @x:
# - @f: full path to the test with slashes replaced with underscores
# - @w: wildcard used as config name
# - @t: target name
# so the workdir .../@f/@w/@t would expand to e.g. tools_test_apps_system_startup/default/esp32

# EXTRA_ARGS="
#   -p ${IOT_SOLUTION_PATH}/examples
#   --build-system ${BUILD_SYSTEM}
#   --target ${IDF_TARGET}
#   --recursive
#   "

# This part of the script produces the same result for all the build jobs.
#
# It may be moved to a separate stage (pre-build) later, then the build jobs
# will receive ${BUILD_LIST_JSON} file as an artifact.
#
# If changing the work-dir or build-dir, remember to update the "artifacts" in
# gitlab-ci configs, and IDFApp.py.

# ${IDF_PATH}/tools/find_apps.py \
#   -vv \
#   --format json \
#   ${WORK_DIR} \
#   --build-dir ${BUILD_DIR} \
#   --build-log ${BUILD_LOG} \
#   --output ${ALL_BUILD_LIST_JSON} \
#   ${EXTRA_ARGS} \
#   ${CONFIG}

# The part below is where the actual builds happen
build_check() {
  NAME=$(basename $1)
  NAME=${NAME%.*}
  JOB_BUILD_LIST_JSON="${BUILD_PATH}/list_job_${CI_NODE_INDEX}_${NAME}.json"
${IOT_SOLUTION_PATH}/tools/build_apps.py \
  -vv \
  --format json \
  --keep-going \
  --parallel-count ${CI_NODE_TOTAL} \
  --parallel-index ${CI_NODE_INDEX} \
  --output-build-list ${JOB_BUILD_LIST_JSON} \
  --size-info ${SIZE_INFO_LOCATION} \
  $1

  # Check for build warnings
  ${IOT_SOLUTION_PATH}/tools/ci/check_build_warnings.py -vv ${JOB_BUILD_LIST_JSON}
}

ALL_BUILD_LIST_JSON=$(ls ${BUILD_PATH})
for f in ${ALL_BUILD_LIST_JSON}
do
  echo -e "\033[1;34m>>> Start to build ${BUILD_PATH}/$f\033[0m"
  build_check ${BUILD_PATH}/$f
done
