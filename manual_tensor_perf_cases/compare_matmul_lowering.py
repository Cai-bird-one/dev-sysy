#!/usr/bin/env python3

import os
import statistics
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKDIR = Path(os.environ.get("TENSOR_MATMUL_WORKDIR",
                              "/tmp/tensor_matmul_compare"))
N = int(os.environ.get("TENSOR_MATMUL_N", "48"))
REPS = int(os.environ.get("TENSOR_MATMUL_REPS", "8"))
RUNS = int(os.environ.get("TENSOR_MATMUL_RUNS", "6"))
SKIP_BUILD = os.environ.get("TENSOR_MATMUL_SKIP_BUILD") == "1"
LIBRARY_PATH = Path(os.environ.get("CDE_LIBRARY_PATH", "/opt/lib"))
IMAGE = os.environ.get("COMPILER_DEV_IMAGE", "maxxing/compiler-dev")


def run(cmd):
  subprocess.run(cmd, check=True)


def in_docker():
  return Path("/.dockerenv").exists()


def rerun_in_docker_if_needed():
  if in_docker() or os.environ.get("TENSOR_MATMUL_HOST") == "1":
    return False
  env_args = []
  for name in ("TENSOR_MATMUL_N", "TENSOR_MATMUL_REPS", "TENSOR_MATMUL_RUNS"):
    if name in os.environ:
      env_args.extend(["-e", f"{name}={os.environ[name]}"])
  run([
      "docker", "run", "--rm",
      "-v", f"{ROOT}:/root/compiler",
      "-w", "/root/compiler",
      *env_args,
      IMAGE,
      "python3", "manual_tensor_perf_cases/compare_matmul_lowering.py",
  ])
  return True


def matrix_initializer(name):
  rows = []
  for i in range(N):
    values = []
    for j in range(N):
      if name == "A":
        values.append(str((i * 17 + j * 13 + 5) % 97))
      else:
        values.append(str((i * 19 + j * 11 + 7) % 89))
    rows.append("{" + ", ".join(values) + "}")
  return "{" + ", ".join(rows) + "}"


def common_header():
  return (
      f"tensor @A: tensor<{N}x{N}>\n"
      f"tensor @B: tensor<{N}x{N}>\n"
      f"tensor @C: tensor<{N}x{N}>\n"
      f"global @A = alloc [[i32, {N}], {N}], {matrix_initializer('A')}\n"
      f"global @B = alloc [[i32, {N}], {N}], {matrix_initializer('B')}\n"
      f"global @C = alloc [[i32, {N}], {N}], zeroinit\n\n")


class KoopaBuilder:
  def __init__(self):
    self.lines = []
    self.next_value = 0

  def emit(self, line):
    self.lines.append(line)

  def value(self, hint):
    name = f"%{hint}{self.next_value}"
    self.next_value += 1
    return name

  def load(self, pointer, hint):
    value = self.value(hint)
    self.emit(f"  {value} = load {pointer}")
    return value

  def binary(self, op, lhs, rhs, hint):
    value = self.value(hint)
    self.emit(f"  {value} = {op} {lhs}, {rhs}")
    return value

  def ptr(self, op, base, index, hint):
    value = self.value(hint)
    self.emit(f"  {value} = {op} {base}, {index}")
    return value


def emit_matrix_ptr(b, base, row, col, hint):
  row_ptr = b.ptr("getelemptr", base, row, f"{hint}_row")
  return b.ptr("getelemptr", row_ptr, col, f"{hint}_elem")


def emit_checksum(b):
  row = emit_matrix_ptr(b, "@C", str(N - 1), str(N - 1), "chk")
  value = b.load(row, "chk")
  old = b.load("%checksum", "checksum")
  total = b.binary("add", old, value, "checksum")
  b.emit(f"  store {total}, %checksum")


def emit_rep_shell(body_lines):
  b = KoopaBuilder()
  b.emit("fun @main(): i32 {")
  b.emit("%entry:")
  b.emit("  %rep = alloc i32")
  b.emit("  %checksum = alloc i32")
  b.emit("  store 0, %rep")
  b.emit("  store 0, %checksum")
  b.emit("  jump %rep_cond")
  b.emit("%rep_cond:")
  rep = b.load("%rep", "rep")
  cond = b.binary("lt", rep, str(REPS), "rep_cmp")
  b.emit(f"  br {cond}, %rep_body, %done")
  b.emit("%rep_body:")
  for line in body_lines:
    b.emit(line)
  emit_checksum(b)
  rep = b.load("%rep", "rep")
  nxt = b.binary("add", rep, "1", "rep_next")
  b.emit(f"  store {nxt}, %rep")
  b.emit("  jump %rep_cond")
  b.emit("%done:")
  out = b.load("%checksum", "out")
  b.emit(f"  ret {out}")
  b.emit("}")
  return "\n".join(b.lines) + "\n"


def tensor_body():
  return [f"  tensor @C = matmul @A, @B : tensor<{N}x{N}>"]


def naive_body():
  b = KoopaBuilder()
  b.emit("  %i = alloc i32")
  b.emit("  %j = alloc i32")
  b.emit("  %k = alloc i32")
  b.emit("  %sum = alloc i32")
  b.emit("  store 0, %i")
  b.emit("  jump %naive_i_cond")
  b.emit("%naive_i_cond:")
  i = b.load("%i", "i")
  icmp = b.binary("lt", i, str(N), "i_cmp")
  b.emit(f"  br {icmp}, %naive_j_start, %naive_done")
  b.emit("%naive_j_start:")
  b.emit("  store 0, %j")
  b.emit("  jump %naive_j_cond")
  b.emit("%naive_j_cond:")
  j = b.load("%j", "j")
  jcmp = b.binary("lt", j, str(N), "j_cmp")
  b.emit(f"  br {jcmp}, %naive_cell_start, %naive_i_next")
  b.emit("%naive_cell_start:")
  b.emit("  store 0, %sum")
  b.emit("  store 0, %k")
  b.emit("  jump %naive_k_cond")
  b.emit("%naive_k_cond:")
  k = b.load("%k", "k")
  kcmp = b.binary("lt", k, str(N), "k_cmp")
  b.emit(f"  br {kcmp}, %naive_k_body, %naive_store")
  b.emit("%naive_k_body:")
  i = b.load("%i", "i")
  j = b.load("%j", "j")
  k = b.load("%k", "k")
  ap = emit_matrix_ptr(b, "@A", i, k, "a")
  bp = emit_matrix_ptr(b, "@B", k, j, "b")
  av = b.load(ap, "a")
  bv = b.load(bp, "b")
  prod = b.binary("mul", av, bv, "prod")
  old = b.load("%sum", "sum")
  total = b.binary("add", old, prod, "sum")
  b.emit(f"  store {total}, %sum")
  k = b.load("%k", "k")
  knext = b.binary("add", k, "1", "k_next")
  b.emit(f"  store {knext}, %k")
  b.emit("  jump %naive_k_cond")
  b.emit("%naive_store:")
  i = b.load("%i", "i")
  j = b.load("%j", "j")
  cp = emit_matrix_ptr(b, "@C", i, j, "c")
  total = b.load("%sum", "sum")
  b.emit(f"  store {total}, {cp}")
  j = b.load("%j", "j")
  jnext = b.binary("add", j, "1", "j_next")
  b.emit(f"  store {jnext}, %j")
  b.emit("  jump %naive_j_cond")
  b.emit("%naive_i_next:")
  i = b.load("%i", "i")
  inext = b.binary("add", i, "1", "i_next")
  b.emit(f"  store {inext}, %i")
  b.emit("  jump %naive_i_cond")
  b.emit("%naive_done:")
  return b.lines


def write_cases():
  cases = {
      "tensor_lowered": common_header() + emit_rep_shell(tensor_body()),
      "naive": common_header() + emit_rep_shell(naive_body()),
  }
  paths = {}
  for name, text in cases.items():
    path = WORKDIR / f"{name}.koopa"
    path.write_text(text, encoding="utf-8")
    paths[name] = path
  return paths


def build_executable(name, source):
  compiler = ROOT / "build/compiler"
  asm = WORKDIR / f"{name}.s"
  obj = WORKDIR / f"{name}.o"
  exe = WORKDIR / f"{name}.exe"
  run([str(compiler), "-perf", str(source), "-o", str(asm)])
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


def run_once(exe):
  start = time.perf_counter()
  result = subprocess.run(["qemu-riscv32-static", str(exe)],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  return (time.perf_counter() - start) * 1000.0, result.returncode


def main():
  if rerun_in_docker_if_needed():
    return
  WORKDIR.mkdir(parents=True, exist_ok=True)
  if not SKIP_BUILD:
    run(["make", "DEBUG=0"])
  sources = write_cases()
  executables = {
      name: build_executable(name, source)
      for name, source in sources.items()
  }

  timings = {name: [] for name in executables}
  returns = {name: set() for name in executables}
  for exe in executables.values():
    run_once(exe)

  names = ["tensor_lowered", "naive"]
  for i in range(RUNS):
    order = names if i % 2 == 0 else list(reversed(names))
    for name in order:
      elapsed, code = run_once(executables[name])
      timings[name].append(elapsed)
      returns[name].add(code)

  print(f"matrix: N={N} reps={REPS} runs={RUNS}")
  for name in names:
    values = timings[name]
    print(
        f"{name}: return={sorted(returns[name])} "
        f"avg_ms={statistics.mean(values):.3f} "
        f"median_ms={statistics.median(values):.3f} "
        f"min_ms={min(values):.3f} max_ms={max(values):.3f}")
  print("ratio tensor_lowered/naive="
        f"{statistics.mean(timings['tensor_lowered']) / statistics.mean(timings['naive']):.4f}")


if __name__ == "__main__":
  main()
