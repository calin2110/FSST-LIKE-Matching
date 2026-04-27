# --- benchmark/CMakeLists.txt ---

find_package(Threads REQUIRED)

set(BENCHMARK_COMMON_HEADERS
        "benchmark_utils.hpp"
)
foreach(bench_name measure_singlethreaded measure_multithreaded measure_decoding)
    add_executable(${bench_name}
            "${bench_name}.cpp"
            ${BENCHMARK_COMMON_HEADERS}
    )

    target_link_libraries(${bench_name}
            PRIVATE core_extended
            PRIVATE Threads::Threads
            PRIVATE PkgConfig::VECTORSCAN
            PRIVATE ${llvm_libs}
            PRIVATE nlohmann_json::nlohmann_json
    )
endforeach()