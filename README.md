# FSST LIKE Matching

This repository contains the implementation of **FSST-LIKE-Matching**, a high-performance string matching engine published at **DaMoN 2026**.
This work is built upon the FSST compression algorithm; the link to the official repository is [https://github.com/cwida/fsst](https://github.com/cwida/fsst).
## Overview

The algorithms in the repository provide efficient pattern matching (specifically `LIKE` patterns) over FSST-compressed data. By avoiding decompression, we achieve high throughput for analytical workloads.

## Repository Structure

The project is organized into several key directories:

*   `include/`: Header files defining the core automata, codegen interfaces, and string search utilities.
*   `src/`: Implementation of the pattern matching engine, including LLVM and C++ code generation.
*   `benchmark/`: Performance measurement suite for datasets including IMDB, StackOverflow, and TPC-H.
*   `test/`: Unit and integration tests for various matching scenarios (start, middle, end, and full pattern matching).
*   `fa-drawing/`: A visualization tool (web-based) to draw and inspect the finite automata used in the matching process.

## Key Features

*   **Automata-based Matching**: Uses specialized finite automata for `LIKE` pattern evaluation.
*   **Codegen Backend**: Supports both LLVM-based JIT compilation and C++ source generation for matching kernels.
*   **FSST Integration**: Direct matching on compressed data without full decompression.
*   **Multithreaded Benchmarking**: Tools to measure throughput across multiple CPU cores.

## Getting Started

### Prerequisites

*   CMake (3.5+)
*   LLVM16 (for LLVM codegen backend)
*   Vectorscan / Hyperscan (for hybrid search support)
*   C++20 compatible compiler

### Build Instructions

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Drawing Automata or Compressing Files

To use the browser client, run the following command:

```bash
cd fa-drawing
./run_server.sh
```

After starting the server, open up `index.html` in a browser. You must provide three values in order to generate the automaton correctly:

* **Pattern**: The `LIKE` pattern to generate the automaton for.
* **Symbol Table Path**: The path to the binary of the symbol tables relative to the main folder of the repository; example symbol tables are provided in `data/`.
* **Automaton Type**: One of four possible values:
    * **start**: For prefixes (the `%` wildcard is implicit at the end).
    * **middle**: For substrings (the `%` wildcard is implicit at both ends).
    * **end**: For suffixes (the `%` wildcard is implicit at the start).
    * **full**: May contain multiple `LIKE` subpatterns and multiple `%` wildcards.
* **Compress File Path**: If you want to generate a symbol table for a custom dataset, enter the path relative to the main folder of the repository. The format of the file is: one string entry per line.

### Running Benchmarks
Once the datasets have been installed, you can run the benchmark scripts by running the following commands:
```bash
cd benchmark
./run_benchmarks.sh
```

[//]: # (## Citation)

[//]: # ()
[//]: # (If you use this work in your research, please cite our DaMoN 2026 paper:)

[//]: # ()
[//]: # (```bibtex)

[//]: # (@inproceedings{fsst-like-damon2026,)

[//]: # (  title={FSST-LIKE-Matching: High-Performance String Matching over Compressed Data},)

[//]: # (  booktitle={Proceedings of the 22nd International Workshop on Data Management on New Hardware &#40;DaMoN '26&#41;},)

[//]: # (  year={2026},)

[//]: # (  url={https://doi.org/10.1145/3789237.3809128})

[//]: # (})

[//]: # (```)

[//]: # ()
[//]: # (---)
