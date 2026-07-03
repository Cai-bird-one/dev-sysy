#!/usr/bin/env python3

import argparse
import importlib.util
from importlib.machinery import SourceFileLoader
import os
import shlex
import subprocess
import sys
import time


def load_autotest():
    spec = importlib.util.spec_from_loader(
        "autotest_impl", SourceFileLoader("autotest_impl", "/opt/bin/autotest")
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load /opt/bin/autotest")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decode_bytes(data):
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return str(data)


def run_program(autotest, case, exe):
    inputs = None
    if case.input_file:
        with open(case.input_file, "rb") as f:
            inputs = f.read()

    command = shlex.split(f"qemu-riscv32-static {exe}")
    start = time.perf_counter()
    result = subprocess.run(
        command,
        timeout=autotest.RUN_TIMEOUT_SEC,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        input=inputs,
    )
    elapsed = time.perf_counter() - start

    stdout = decode_bytes(result.stdout)
    stderr = decode_bytes(result.stderr)
    if not stdout or stdout.endswith("\n"):
        actual = f"{stdout}{result.returncode}"
    else:
        actual = f"{stdout}\n{result.returncode}"

    with open(case.output_file, mode="r", newline="") as f:
        expected = f.read().rstrip()

    passed = actual == expected
    return passed, elapsed, actual, expected, stderr


def run_case(autotest, compiler, case):
    output = os.path.join(compiler.working_dir, autotest.TEMP_OUTPUT_FILE)
    obj = os.path.join(compiler.working_dir, autotest.TEMP_OBJECT_FILE)
    exe = os.path.join(compiler.working_dir, autotest.TEMP_EXECUTABLE_FILE)

    compile_cmd = f'{compiler.compile_cmd} -perf {case.source_file} -o {output}'
    result = autotest.execute(
        compile_cmd,
        autotest.COMP_TIMEOUT_SEC,
        autotest.TestStatus.COMP_ERROR,
        autotest.TestStatus.COMP_TIME_EXCEEDED,
    )
    if result:
        return False, 0.0, "compile", result
    if not os.path.exists(output):
        return False, 0.0, "output", None

    result = autotest.asm_riscv(output, obj, exe)
    if result:
        return False, 0.0, "assemble", result

    try:
        passed, elapsed, actual, expected, stderr = run_program(autotest, case, exe)
    except subprocess.TimeoutExpired:
        return False, autotest.RUN_TIMEOUT_SEC, "run-timeout", None
    if not passed:
        return False, elapsed, "wrong-answer", (actual, expected, stderr)
    return True, elapsed, "passed", None


def main():
    parser = argparse.ArgumentParser(
        description="Measure only qemu RISC-V program runtime for autotest cases."
    )
    parser.add_argument("repo_dir")
    parser.add_argument("-t", "--test_case_dir", required=True)
    parser.add_argument("-s", "--sub_dir")
    parser.add_argument("-w", "--working_dir")
    parser.add_argument("--case", dest="case_name")
    parser.add_argument("--top", type=int, default=12)
    args = parser.parse_args()

    autotest = load_autotest()
    test_case_dir = os.path.abspath(args.test_case_dir)
    if args.sub_dir:
        test_case_dir = os.path.join(test_case_dir, args.sub_dir)
    cases = autotest.scan_test_cases(test_case_dir)
    if args.case_name:
        cases = [case for case in cases if case.name == args.case_name]
    if not cases:
        print(f"no test cases found in {test_case_dir}", file=sys.stderr)
        return 2

    compiler = autotest.build_repo(args.repo_dir, args.working_dir)
    if not compiler:
        return 2

    total = 0.0
    passed = 0
    timings = []
    try:
        for case in cases:
            ok, elapsed, status, detail = run_case(autotest, compiler, case)
            timings.append((elapsed, case.name, status))
            if ok:
                passed += 1
                total += elapsed
                print(f"PASS {elapsed:.6f}s {case.name}", flush=True)
            else:
                print(f"FAIL {elapsed:.6f}s {case.name} {status}", flush=True)
                if detail is not None:
                    print(detail, file=sys.stderr)
                return 1
    finally:
        compiler.clean()

    print(f"SUMMARY passed={passed}/{len(cases)} runtime={total:.6f}s")
    print("SLOWEST")
    for elapsed, name, status in sorted(timings, reverse=True)[: args.top]:
        print(f"{elapsed:.6f}s {status} {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
