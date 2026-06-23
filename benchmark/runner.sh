  for i in {1..1};do
    for dataset in "${datasets[@]}";do
      for algo in "${algorithms[@]}";do
        echo "Measuring Multithreaded Times: Iteration $i, Dataset $dataset, Algorithm $algo"
        ../build/benchmark/measure_multithreaded "$dataset" "$algo" "0" "$i"
      done
    done
  done