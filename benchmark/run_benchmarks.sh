#!/bin/bash

datasets=("IMDB" "StackOverflow" "TPCH" "PublicBI")
algorithms=("InMemory" "SSELLVMCompiled" "SSECppCompiled" "SSEDecoded" "VectorScan")

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

build_benchmarks() {
  echo "Project Root identified as: $PROJECT_ROOT"

  # Configure if build folder doesn't exist
  if [ ! -d "$BUILD_DIR" ]; then
     # Run cmake from the root
     cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S "$PROJECT_ROOT" -B "$BUILD_DIR"
  fi

  echo "Building benchmark targets..."
  cmake --build "$BUILD_DIR" --target measure_decoding measure_singlethreaded measure_multithreaded -j $(nproc)
  return $?
}

run_decoding() {
  for i in {1..10}; do
    for dataset in "${datasets[@]}"; do
      echo "--- Iteration $i | Dataset: $dataset ---"
      ../build/benchmark/measure_decoding "$dataset" "$i"
    done
  done
}

run_singlethreaded() {
  for i in {1..10};do
    for dataset in "${datasets[@]}";do
      for algo in "${algorithms[@]}";do
        echo "Measuring Singlethreaded Times: Iteration $i, Dataset $dataset, Algorithm $algo"
        ../build/benchmark/measure_singlethreaded "$dataset" "$algo" "0" "$i"
      done
    done
  done
}

run_multithreaded() {
  for i in {1..10};do
    for dataset in "${datasets[@]}";do
      for algo in "${algorithms[@]}";do
        echo "Measuring Multithreaded Times: Iteration $i, Dataset $dataset, Algorithm $algo"
        ../build/benchmark/measure_multithreaded "$dataset" "$algo" "0" "$i"
      done
    done
  done
}

build_benchmarks && {
  echo "Build successful. Starting benchmark suite..."

  run_decoding
  run_singlethreaded
  run_multithreaded

  echo "All benchmarks completed successfully."
} || {
  echo "Build failed. Benchmarks aborted."
  exit 1
}