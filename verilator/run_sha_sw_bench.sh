#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"

mkdir -p testlogs

log_file="${script_dir}/testlogs/test_sha256_sw_bench_bg.log"
exit_file="${script_dir}/testlogs/test_sha256_sw_bench_bg.exit"

rm -f "${log_file}" "${exit_file}"

set +e
timeout 900 ./obj_dir/Vtb_croc_soc +binary=../sw/bin/test/test_sha256_sw_bench.hex >"${log_file}" 2>&1
rc=$?
set -e

echo "${rc}" > "${exit_file}"
exit "${rc}"
