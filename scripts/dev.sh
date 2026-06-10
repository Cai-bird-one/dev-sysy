#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
IMAGE="${COMPILER_DEV_IMAGE:-maxxing/compiler-dev}"
WORKDIR="${COMPILER_DEV_WORKDIR:-/root/compiler}"

usage() {
  cat <<EOF
Usage: $0 <command> [args...]

Commands:
  build [make-args...]       Build the compiler in Docker
  clean [make-args...]       Clean build artifacts in Docker
  test [make-args...]        Build and run unit tests in Docker
  lex <source-file>          Print the token stream for a source file
  parse <source-file>        Print the parse tree for a source file
  ir <source-file>           Print Koopa IR for a source file
  run <compiler-args...>     Run build/compiler in Docker
  shell                      Open an interactive shell in Docker

Examples:
  $0 build
  $0 build DEBUG=0
  $0 test
  $0 lex test_codes/main.cpp
  $0 parse test_codes/main.cpp
  $0 ir test_codes/main.cpp
  $0 run -koopa input.sy -o build/output.koopa
  $0 clean
EOF
}

docker_run() {
  docker run --rm \
    -v "${ROOT_DIR}:${WORKDIR}" \
    -w "${WORKDIR}" \
    "${IMAGE}" \
    "$@"
}

docker_run_as_root() {
  docker run --rm \
    -v "${ROOT_DIR}:${WORKDIR}" \
    -w "${WORKDIR}" \
    "${IMAGE}" \
    "$@"
}

docker_shell() {
  docker run --rm -it \
    -v "${ROOT_DIR}:${WORKDIR}" \
    -w "${WORKDIR}" \
    "${IMAGE}" \
    bash
}

command="${1:-help}"
if [[ $# -gt 0 ]]; then
  shift
fi

case "${command}" in
  build)
    docker_run make "$@"
    ;;
  clean)
    docker_run_as_root make clean "$@"
    ;;
  test)
    docker_run make test "$@"
    ;;
  lex)
    if [[ $# -ne 1 ]]; then
      echo "Usage: $0 lex <source-file>" >&2
      exit 1
    fi
    if [[ ! -f "${ROOT_DIR}/$1" ]]; then
      echo "source file not found: $1" >&2
      exit 1
    fi
    docker_run bash -lc \
      'clang++ -std=c++17 -Isrc tools/dump_tokens.cpp src/compiler/lexer/lexer.cpp src/compiler/lexer/automaton.cpp src/compiler/lexer/regex.cpp src/compiler/lexer/token_rules.cpp -o /tmp/dump_tokens && /tmp/dump_tokens "$1"' \
      bash "$1"
    ;;
  parse)
    if [[ $# -ne 1 ]]; then
      echo "Usage: $0 parse <source-file>" >&2
      exit 1
    fi
    if [[ ! -f "${ROOT_DIR}/$1" ]]; then
      echo "source file not found: $1" >&2
      exit 1
    fi
    docker_run bash -lc \
      'clang++ -std=c++17 -Isrc tools/dump_parse_tree.cpp src/compiler/lexer/lexer.cpp src/compiler/lexer/automaton.cpp src/compiler/lexer/regex.cpp src/compiler/lexer/token_rules.cpp src/compiler/parser/parser.cpp src/compiler/parser/parser_analysis.cpp src/compiler/parser/parse_tree.cpp src/compiler/parser/grammar_rules.cpp -o /tmp/dump_parse_tree && /tmp/dump_parse_tree "$1"' \
      bash "$1"
    ;;
  ir)
    if [[ $# -ne 1 ]]; then
      echo "Usage: $0 ir <source-file>" >&2
      exit 1
    fi
    if [[ ! -f "${ROOT_DIR}/$1" ]]; then
      echo "source file not found: $1" >&2
      exit 1
    fi
    docker_run bash -lc \
      'clang++ -std=c++17 -Isrc tools/dump_koopa.cpp src/compiler/lexer/lexer.cpp src/compiler/lexer/automaton.cpp src/compiler/lexer/regex.cpp src/compiler/lexer/token_rules.cpp src/compiler/parser/parser.cpp src/compiler/parser/parser_analysis.cpp src/compiler/parser/parse_tree.cpp src/compiler/parser/grammar_rules.cpp src/compiler/ir/koopa_generator.cpp src/compiler/ir/semantic.cpp -o /tmp/dump_koopa && /tmp/dump_koopa "$1"' \
      bash "$1"
    ;;
  run)
    if [[ ! -x "${ROOT_DIR}/build/compiler" ]]; then
      docker_run make
    fi
    docker_run ./build/compiler "$@"
    ;;
  shell)
    docker_shell
    ;;
  help|-h|--help)
    usage
    ;;
  *)
    echo "Unknown command: ${command}" >&2
    usage >&2
    exit 1
    ;;
esac
