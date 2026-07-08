#!/usr/bin/env python3

import argparse
import os
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / "compiler2021/公开用例与运行时库/performance_test2021-public"
PRIVATE = ROOT / "compiler2021/公开用例与运行时库/performance_test2021-private"
WORKDIR = Path(os.environ.get("SYSY2021_PERF_WORKDIR", "/tmp/sysy2021_perf"))
IMAGE = os.environ.get("COMPILER_DEV_IMAGE", "maxxing/compiler-dev")
LIBRARY_PATH = Path(os.environ.get("CDE_LIBRARY_PATH", "/opt/lib"))


@dataclass
class Case:
  name: str
  source: Path
  input: Path
  expected: Path


def public_case(name, stem):
  return Case(name, PUBLIC / f"{stem}.sy", PUBLIC / f"{stem}.in",
              PUBLIC / f"{stem}.out")


def private_case(name, stem):
  return Case(name, PRIVATE / f"{stem}.sy", PRIVATE / f"{stem}.in",
              PRIVATE / f"{stem}.out")


CASES = [
    public_case("00_bitset1", "00_bitset1"),
    public_case("01_bitset2", "00_bitset2"),
    public_case("02_bitset3", "00_bitset3"),
    public_case("03_mm1", "01_mm1"),
    public_case("04_mm2", "01_mm2"),
    public_case("05_mm3", "01_mm3"),
    public_case("06_mv1", "02_mv1"),
    public_case("07_mv2", "02_mv2"),
    public_case("08_mv3", "02_mv3"),
    public_case("09_spmv1", "04_spmv1"),
    public_case("10_spmv2", "04_spmv2"),
    public_case("11_spmv3", "04_spmv3"),
    public_case("12_fft0", "fft0"),
    public_case("13_fft1", "fft1"),
    public_case("14_fft2", "fft2"),
    public_case("15_transpose0", "transpose0"),
    public_case("16_transpose1", "transpose1"),
    public_case("17_transpose2", "transpose2"),
    private_case("18_brainfuck-bootstrap", "brainfuck-bootstrap"),
    private_case("19_brainfuck-calculator", "brainfuck-calculator"),
]


def run(cmd, **kwargs):
  return subprocess.run(cmd, check=True, **kwargs)


def in_docker():
  return Path("/.dockerenv").exists()


def rerun_in_docker_if_needed(args):
  if in_docker() or os.environ.get("SYSY2021_PERF_HOST") == "1":
    return False
  cmd = [
      "docker", "run", "--rm",
      "-v", f"{ROOT}:/root/compiler",
      "-w", "/root/compiler",
      IMAGE,
      "python3", "scripts/run_2021_perf_cases.py",
      *args,
  ]
  run(cmd)
  return True


def existing_cases(selected):
  selected_set = set(selected or [])
  cases = []
  skipped = []
  for case in CASES:
    if selected_set and case.name not in selected_set:
      continue
    missing = [p for p in (case.source, case.input, case.expected)
               if not p.exists()]
    if missing:
      skipped.append((case, missing))
    else:
      cases.append(case)
  return cases, skipped


def extract_cases(cases, target):
  target.mkdir(parents=True, exist_ok=True)
  manifest = []
  for case in cases:
    case_dir = target / case.name
    case_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(case.source, case_dir / case.source.name)
    shutil.copy2(case.input, case_dir / case.input.name)
    shutil.copy2(case.expected, case_dir / case.expected.name)
    manifest.append(f"{case.name},{case.source},{case.input},{case.expected}\n")
  (target / "manifest.csv").write_text("".join(manifest), encoding="utf-8")


def build_executable(case):
  compiler = ROOT / "build/compiler"
  asm = WORKDIR / f"{case.name}.s"
  obj = WORKDIR / f"{case.name}.o"
  exe = WORKDIR / f"{case.name}.exe"
  run([str(compiler), "-perf", str(case.source), "-o", str(asm)])
  run([
      "clang",
      str(asm),
      "-c",
      "-o",
      str(obj),
      "-target",
      "riscv32-unknown-linux-elf",
      "-march=rv32im",
      "-mabi=ilp32",
  ])
  run(["ld.lld", str(obj), f"-L{LIBRARY_PATH / 'riscv32'}", "-lsysy", "-o",
       str(exe)])
  return exe


def run_case(case, exe, runs):
  stdin = case.input.read_bytes()
  expected = case.expected.read_bytes()
  timings = []
  stdout = b""
  stderr = b""
  code = 0
  for _ in range(runs):
    start = time.perf_counter()
    result = subprocess.run(["qemu-riscv32-static", str(exe)],
                            input=stdin,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            timeout=300)
    timings.append(time.perf_counter() - start)
    stdout = result.stdout
    stderr = result.stderr
    code = result.returncode
  ok = code == 0 and stdout.rstrip() == expected_stdout(expected, code)
  return ok, code, timings, stdout, expected, stderr


def expected_stdout(expected, code):
  text = expected.rstrip()
  suffix = str(code).encode()
  if text == suffix:
    return b""
  if text.endswith(b"\n" + suffix):
    return text[:-(len(suffix) + 1)].rstrip()
  return text


def first_difference(left, right):
  limit = min(len(left), len(right))
  for index in range(limit):
    if left[index] != right[index]:
      return index
  if len(left) != len(right):
    return limit
  return -1


def parse_args():
  parser = argparse.ArgumentParser(
      description="Run the local 2021 SysY performance subset.")
  parser.add_argument("--runs", type=int, default=1)
  parser.add_argument("--skip-build", action="store_true")
  parser.add_argument("--extract", type=Path)
  parser.add_argument("--case", action="append", default=[])
  return parser.parse_args()


def main():
  if rerun_in_docker_if_needed(sys.argv[1:]):
    return
  args = parse_args()
  WORKDIR.mkdir(parents=True, exist_ok=True)
  cases, skipped = existing_cases(args.case)
  if args.extract:
    extract_cases(cases, args.extract)
    print(f"extracted {len(cases)} cases to {args.extract}")
  for case, missing in skipped:
    names = ", ".join(str(p) for p in missing)
    print(f"SKIP {case.name}: missing {names}")
  if not cases:
    return
  if not args.skip_build:
    run(["make", "DEBUG=0"])

  rows = []
  for case in cases:
    exe = build_executable(case)
    ok, code, timings, actual, expected, stderr = run_case(case, exe,
                                                           args.runs)
    avg = statistics.mean(timings)
    rows.append((case.name, ok, code, avg, timings))
    status = "AC" if ok else "WA"
    print(f"{case.name}: {status} avg={avg:.6f}s runs="
          f"{','.join(f'{t:.6f}' for t in timings)}")
    if not ok:
      diff = first_difference(actual, expected)
      print(f"  return={code} stdout={actual[:120]!r} expected={expected[:120]!r}")
      print(f"  len stdout={len(actual)} expected={len(expected)} first_diff={diff}")
      if diff >= 0:
        begin = max(0, diff - 40)
        end = diff + 80
        print(f"  stdout_diff_window={actual[begin:end]!r}")
        print(f"  expect_diff_window={expected[begin:end]!r}")
      print(f"  stderr={stderr[:200]!r}")
      raise SystemExit(1)

  total = sum(avg for _, _, _, avg, _ in rows)
  print(f"TOTAL avg={total:.6f}s cases={len(rows)} runs={args.runs}")


if __name__ == "__main__":
  main()
