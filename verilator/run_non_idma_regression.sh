#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${script_dir}"

mkdir -p testlogs

fail=0

for bin in ../sw/bin/helloworld.hex ../sw/bin/test/*.hex; do
  name="$(basename "${bin}" .hex)"
  log="testlogs/${name}.log"

  if [[ "${name}" == "test_idma" ]]; then
    echo "SKIP ${name}"
    continue
  fi

  timeout_s=300
  if [[ "${name}" == "test_sha256_accel_end_to_end" || "${name}" == "test_sha256_sw_bench" ]]; then
    timeout_s=900
  fi
  if timeout "${timeout_s}" ./obj_dir/Vtb_croc_soc +binary="${bin}" >"${log}" 2>&1; then
    if grep -q "Simulation finished: SUCCESS" "${log}"; then
      echo "PASS ${name}"
    else
      echo "FAIL_NO_SUCCESS ${name}"
      tail -n 40 "${log}" || true
      fail=1
    fi
  else
    code=$?
    echo "FAIL_EXIT_${code} ${name}"
    tail -n 40 "${log}" || true
    fail=1
  fi
done

exit "${fail}"
