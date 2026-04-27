import pandas as pd
from collections import defaultdict

def denormalize_file(path, patterns, times, emptyQueryNum, dataset):
    df = pd.read_csv(path, delimiter=',')
    for row in df.itertuples(index=True, name='Row'):
        queryNum = '' if emptyQueryNum else row.queryNum
        table = row.table
        pattern = row.pattern
        currentPatternKey = (dataset, queryNum, table, pattern)
        if currentPatternKey not in patterns:
            patterns[currentPatternKey] =  {
                'patternId': len(patterns),
                'dataset': dataset,
                'queryNum': queryNum,
                'table': table,
                'pattern': pattern
            }
        patternId = patterns[currentPatternKey]['patternId']

        scaleFactor = getattr(row, 'scaleFactor', 0)
        numLines = row.numLines
        runNumber = row.runNumber
        numBytes = row.numBytes
        currentTimeKey = (patternId, runNumber, scaleFactor, numLines, numBytes)
        if currentTimeKey not in times:
            times[currentTimeKey] = {
                'patternId': patternId,
                'runNumber': runNumber,
                'scaleFactor': scaleFactor,
                'numLines': numLines,
                'numBytes': numBytes,
                'HSSCreate': None,
                'HSSRun': None,
                'SSECreate': None,
                'SSERun': None,
                'AutomatonCreate': None,
                'InterpretedRun': None,
                'NoSSECppCompile': None,
                'NoSSECppRun': None,
                'SSECppCompile': None,
                'SSECppRun': None,
                'NoSSELLVMCompile': None,
                'NoSSELLVMRun': None,
                'SSELLVMCompile': None,
                'SSELLVMRun': None,
                'VectorScanCompile': None,
                'VectorScanRun': None
            }

        algorithm = row.algorithm
        preprocessTime = row.preprocessTime
        compileTime = row.compileTime
        runTime = row.runTime

        match algorithm:
            case "HSSDecoded":
                times[currentTimeKey]['HSSCreate'] = preprocessTime
                times[currentTimeKey]['HSSRun'] = runTime

            case "SSEDecoded":
                times[currentTimeKey]['SSECreate'] = preprocessTime
                times[currentTimeKey]['SSERun'] = runTime

            case "InMemory":
                times[currentTimeKey]['InterpretedRun'] = runTime
                if not times[currentTimeKey]['AutomatonCreate'] or preprocessTime > times[currentTimeKey]['AutomatonCreate']:
                    times[currentTimeKey]['AutomatonCreate'] = preprocessTime

            case "NoSSECppCompiled":
                times[currentTimeKey]['NoSSECppCompile'] = compileTime
                times[currentTimeKey]['NoSSECppRun'] = runTime
                if not times[currentTimeKey]['AutomatonCreate'] or preprocessTime > times[currentTimeKey]['AutomatonCreate']:
                    times[currentTimeKey]['AutomatonCreate'] = preprocessTime

            case "SSECppCompiled":
                times[currentTimeKey]['SSECppCompile'] = compileTime
                times[currentTimeKey]['SSECppRun'] = runTime
                if not times[currentTimeKey]['AutomatonCreate'] or preprocessTime > times[currentTimeKey]['AutomatonCreate']:
                    times[currentTimeKey]['AutomatonCreate'] = preprocessTime

            case "NoSSELLVMCompiled":
                times[currentTimeKey]['NoSSELLVMCompile'] = compileTime
                times[currentTimeKey]['NoSSELLVMRun'] = runTime
                if not times[currentTimeKey]['AutomatonCreate'] or preprocessTime > times[currentTimeKey]['AutomatonCreate']:
                    times[currentTimeKey]['AutomatonCreate'] = preprocessTime

            case "SSELLVMCompiled":
                times[currentTimeKey]['SSELLVMCompile'] = compileTime
                times[currentTimeKey]['SSELLVMRun'] = runTime
                if not times[currentTimeKey]['AutomatonCreate'] or preprocessTime > times[currentTimeKey]['AutomatonCreate']:
                    times[currentTimeKey]['AutomatonCreate'] = preprocessTime

            case "VectorScan":
                times[currentTimeKey]['VectorScanCompile'] = preprocessTime
                times[currentTimeKey]['VectorScanRun'] = runTime

def sanity_check_null_values(csv_path):
    df = pd.read_csv(csv_path)
    cols_to_check = [col for col in df.columns if col != 'queryNum']
    error_mask = df[cols_to_check].isnull().any(axis=1)
    null_rows = df[error_mask]

    if null_rows.empty:
        print(f"✅ Sanity Check Passed: No null values detected (ignoring 'queryNum') for {csv_path}.")
    else:
        num_errors = len(null_rows)
        print(f"❌ Sanity Check Failed: Found {num_errors} row(s) with null values for {csv_path}.")

def sanity_check_consistency(csv_path):
    df = pd.read_csv(csv_path)
    all_possible_groupers = ['scaleFactor', 'table', 'column', 'schema', 'pattern']
    group_cols = [col for col in all_possible_groupers if col in df.columns]

    if not group_cols:
        print("⚠️ No valid grouping columns found.")
        return pd.DataFrame()

    summary = df.groupby(group_cols + ['numMatches'])['algorithm'].unique().reset_index()
    config_counts = summary.groupby(group_cols).size()
    inconsistent_configs = config_counts[config_counts > 1].index

    report = summary[summary.set_index(group_cols).index.isin(inconsistent_configs)]

    if report.empty:
        print("✅ Success: All algorithms produced identical results for all configurations.")
        return None
    else:
        print(f"❌ Inconsistency Detected: {len(inconsistent_configs)} configs have conflicting results.")
        report = report.rename(columns={'algorithm': 'approaches_with_this_count'})
        return report

def denormalize_files(tpch_path, stackoverflow_path, imdb_path, publicbi_path, out_patterns_path, out_time_path):
    patterns = {}
    times = {}
    denormalize_file(tpch_path, patterns, times, False, 'TPCH')
    denormalize_file(stackoverflow_path, patterns, times, True, 'StackOverflow')
    denormalize_file(imdb_path, patterns, times, True, 'IMDB')
    denormalize_file(publicbi_path, patterns, times, True, 'PublicBI')

    df = pd.DataFrame.from_dict(patterns, orient="index")
    df.to_csv(out_patterns_path, index=False, header=True)

    time_df = pd.DataFrame.from_dict(times, orient="index")
    time_df.to_csv(out_time_path, index=False, header=True)

if __name__ == "__main__":
    in_paths = {
        "TPCH": "../data/tpch.csv",
        "StackOverflow": "../data/stackoverflow.csv",
        "IMDB": "../data/imdb.csv",
        "PublicBI": "../data/publicbi.csv"
    }

    # 1. Improved Error Checking Loop
    for key, csv_path in in_paths.items():
        sanity_check_null_values(csv_path)
        ret = sanity_check_consistency(csv_path)
        if ret is not None:
            print(ret)

    out_paths = {
        "patterns": "../data/patterns.csv",
        "time": "../data/time.csv"
    }

    denormalize_files(
        tpch_path=in_paths["TPCH"],
        stackoverflow_path=in_paths["StackOverflow"],
        imdb_path=in_paths["IMDB"],
        publicbi_path=in_paths["PublicBI"],
        out_patterns_path=out_paths["patterns"],
        out_time_path=out_paths["time"]
    )
