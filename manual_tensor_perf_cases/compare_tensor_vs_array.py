#!/usr/bin/env python3

import hashlib
import os
import statistics
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKDIR = Path(os.environ.get("TENSOR_PERF_WORKDIR", "/tmp/tensor_perf_compare"))
RUNS = int(os.environ.get("TENSOR_PERF_RUNS", "10"))
LIBRARY_PATH = Path(os.environ.get("CDE_LIBRARY_PATH", "/opt/lib"))

CASES = {
    "tensor": ROOT / "manual_tensor_perf_cases/tensor/matrix_accumulate.sy",
    "array": ROOT / "manual_tensor_perf_cases/array/matrix_accumulate.sy",
}


def run(cmd):
  subprocess.run(cmd, check=True)


def sha256(path):
  digest = hashlib.sha256()
  with path.open("rb") as f:
    for chunk in iter(lambda: f.read(65536), b""):
      digest.update(chunk)
  return digest.hexdigest()


def line_count(path):
  with path.open("r", encoding="utf-8") as f:
    return sum(1 for _ in f)


def build_assembly():
  compiler = ROOT / "build/compiler"
  run(["make", "DEBUG=0", "-C", str(ROOT)])

  asm_paths = {}
  for name, source in CASES.items():
    asm = WORKDIR / f"{name}.s"
    run([str(compiler), "-perf", str(source), "-o", str(asm)])
    asm_paths[name] = asm
  return asm_paths


def build_executable(name, asm):
  obj = WORKDIR / f"{name}.o"
  exe = WORKDIR / f"{name}.exe"
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
  run(["ld.lld", str(obj), f"-L{LIBRARY_PATH / 'riscv32'}", "-lsysy", "-o", str(exe)])
  return exe


def run_once(exe):
  start = time.perf_counter()
  result = subprocess.run(
      ["qemu-riscv32-static", str(exe)],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  elapsed_ms = (time.perf_counter() - start) * 1000.0
  return elapsed_ms, result.returncode


def main():
  WORKDIR.mkdir(parents=True, exist_ok=True)

  asm_paths = build_assembly()
  hashes = {name: sha256(path) for name, path in asm_paths.items()}
  lines = {name: line_count(path) for name, path in asm_paths.items()}
  identical = hashes["tensor"] == hashes["array"]

  exe_paths = {
      name: build_executable(name, asm)
      for name, asm in asm_paths.items()
  }

  timings = {name: [] for name in CASES}
  returns = {name: set() for name in CASES}

  for name, exe in exe_paths.items():
    run_once(exe)

  for i in range(RUNS):
    order = ("tensor", "array") if i % 2 == 0 else ("array", "tensor")
    for name in order:
      elapsed_ms, code = run_once(exe_paths[name])
      timings[name].append(elapsed_ms)
      returns[name].add(code)

  print("assembly:")
  for name in ("tensor", "array"):
    print(f"  {name}: sha256={hashes[name]} lines={lines[name]}")
  print(f"  identical={str(identical).lower()}")

  print("runtime:")
  for name in ("tensor", "array"):
    values = timings[name]
    print(
        f"  {name}: return={sorted(returns[name])} "
        f"avg_ms={statistics.mean(values):.3f} "
        f"median_ms={statistics.median(values):.3f} "
        f"min_ms={min(values):.3f} "
        f"max_ms={max(values):.3f}")

  avg_tensor = statistics.mean(timings["tensor"])
  avg_array = statistics.mean(timings["array"])
  print(f"  tensor/array avg ratio={avg_tensor / avg_array:.4f}")


if __name__ == "__main__":
  main()
