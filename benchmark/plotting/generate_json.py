import math
from collections import defaultdict
import pandas as pd
import json
from math import ceil


def format_number(num):
    abs_num = abs(num)

    if abs_num >= 1_000_000_000:  # Billions
        return f"{num / 1_000_000_000:.2f}B"
    elif abs_num >= 1_000_000:    # Millions
        return f"{num / 1_000_000:.2f}M"
    elif abs_num >= 1_000:        # Thousands
        return f"{num / 1_000:.2f}K"
    else:                         # Less than 1000
        return f"{num:.2f}"

# TODO: this is for only 0 underscores. A previous run was working for multiple underscores
def get_statistic(run_df, time_df, pattern_type, ret):
    df = pd.merge(run_df, time_df, on='patternId', how='inner')
    df['InterpretedSpeedup'] = df['SSERun'] / df['InterpretedRun']
    df['CppSpeedup'] = df['SSERun'] / df['SSECppRun']
    df['LLVMSpeedup'] = df['SSERun'] / df['SSELLVMRun']
    df['CppCompileTuple'] = df['SSECppCompile'] / (df['SSECppRun'] / df['numLines'])
    df['LLVMCompileTuple'] = df['SSELLVMCompile'] / (df['SSELLVMRun'] / df['numLines'])
    df['CppCreateTuple'] = df['AutomatonCreate'] / (df['SSECppRun'] / df['numLines'])
    df['LLVMCreateTuple'] = df['AutomatonCreate'] / (df['SSELLVMRun'] / df['numLines'])
    df['InterpretedCreateTuple'] = df['AutomatonCreate'] / (df['InterpretedRun'] / df['numLines'])

    dataset_groups = df.groupby('dataset')
    for dataset, group in dataset_groups:
        cols = [
            'InterpretedSpeedup', 'CppSpeedup', 'LLVMSpeedup',
            'CppCompileTuple', 'LLVMCompileTuple', 'CppCreateTuple',
            'LLVMCreateTuple', 'InterpretedCreateTuple'
        ]
        pattern_groups = group.groupby('pattern')[cols].mean()
        ret[pattern_type][dataset] = {
            "avg_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].mean() * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].mean() * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].mean() * 100)}%"
            },
            "median_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].median() * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].median() * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].median() * 100)}%"
            },
            "min_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].min() * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].min() * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].min() * 100)}%"
            },
            "75th_quantile_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].quantile(0.25) * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].quantile(0.25) * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].quantile(0.25) * 100)}%"
            },
            "95th_quantile_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].quantile(0.05) * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].quantile(0.05) * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].quantile(0.05) * 100)}%"
            },
            "max_speedup": {
                "Interpreted": f"{round(pattern_groups['InterpretedSpeedup'].max() * 100)}%",
                "Cpp": f"{round(pattern_groups['CppSpeedup'].max() * 100)}%",
                "LLVM": f"{round(pattern_groups['LLVMSpeedup'].max() * 100)}%"
            },
            "median_tuples_per_compile": {
                "Cpp": format_number(ceil(pattern_groups['CppCompileTuple'].median())),
                "LLVM": format_number(ceil(pattern_groups['LLVMCompileTuple'].median())),
            },
            "median_tuples_per_create": {
                "Cpp": ceil(pattern_groups['CppCreateTuple'].median()),
                "LLVM": ceil(pattern_groups['LLVMCreateTuple'].median()),
                "Interpreted": ceil(pattern_groups['InterpretedCreateTuple'].median())
            }
        }

def get_statistic_for_pattern_type(run_df, time_df, type, ret):
    get_statistic(run_df=run_df, time_df=time_df, pattern_type=type, ret=ret)

def get_full_statistic():
    run_df = pd.read_csv("../data/patterns.csv")
    time_df = pd.read_csv("../data/time.csv")
    time_df = time_df.groupby(['patternId', 'scaleFactor', 'numLines']).median(numeric_only=True).reset_index()
    ret = defaultdict(lambda: defaultdict(dict))
    start_run_df = run_df[~run_df['pattern'].str.startswith('%')]
    get_statistic_for_pattern_type(start_run_df, time_df, 'start', ret)

    end_run_df = run_df[~run_df['pattern'].str.endswith('%')]
    get_statistic_for_pattern_type(end_run_df, time_df, 'end', ret)

    middle_run_df = run_df[run_df['pattern'].str.startswith('%') & run_df['pattern'].str.endswith('%')]
    get_statistic_for_pattern_type(middle_run_df, time_df, 'middle', ret)
    ret = dict(ret)
    with open("../data/results.json", "w") as f:
        json.dump(ret, f, indent=4)


def get_compile_statistics():
    run_df = pd.read_csv("data/runs.csv")
    time_df = pd.read_csv("../data/time.csv")
    time_df['HSSRuntimePerTuple'] = time_df['SSERun'] / time_df['numLines']
    time_df['LLVMRuntimePerTuple'] = time_df['SSELLVMRun'] / time_df['numLines']
    time_df['InterpretedRuntimePerTuple'] = time_df['InterpretedRun'] / time_df['numLines']
    time_df['CppRuntimePerTuple'] = time_df['SSECppRun'] / time_df['numLines']
    time_df['TuplesUntilCppCompilation'] = (time_df['SSECppCompile'] - time_df['SSECreate']) / time_df['HSSRuntimePerTuple']
    time_df['TuplesUntilLLVMCompilation'] = (time_df['SSELLVMCompile'] - time_df['SSECreate']) / time_df['HSSRuntimePerTuple']
    time_df['TuplesUntilAutomatonCreation'] = (time_df['AutomatonCreate'] - time_df['SSECreate']) / time_df['HSSRuntimePerTuple']
    time_df = time_df.groupby(['patternId', 'scaleFactor', 'numLines']).median(numeric_only=True).reset_index()
    df = pd.merge(run_df, time_df, on='runId', how='inner')
    df['column'] = df['column'].fillna('')
    grouped = df.groupby('dataset')
    ret = {}
    for name, dataset_groups in grouped:
        ret_for_dataset = {}
        sfs = sorted(dataset_groups['scaleFactor'].unique())

        for sf in sfs:
            ret_for_sf = {}
            sf_df = dataset_groups[dataset_groups['scaleFactor'] == sf]
            grouped2 = sf_df.groupby(["column", "table"])
            for group_name, coltable_groups in grouped2:
                ret_for_coltable = {}
                median_vals = coltable_groups.median(numeric_only=True)
                ret_for_coltable['TuplesUntilCppCompilation'] = ceil(median_vals['TuplesUntilCppCompilation'])
                ret_for_coltable['TuplesUntilLLVMCompilation'] = ceil(median_vals['TuplesUntilLLVMCompilation'])
                ret_for_coltable['TuplesUntilAutomatonCreation'] = ceil(median_vals['TuplesUntilAutomatonCreation'])
                column = group_name[0]
                table = group_name[1]
                if column != '':
                    if table not in ret_for_sf:
                        ret_for_sf[table] = {}
                    ret_for_sf[table][column] = ret_for_coltable
                else:
                    ret_for_sf[table] = ret_for_coltable

            ret_for_dataset[sf] = ret_for_sf
        ret[name] = ret_for_dataset

    ret = dict(ret)
    with open("data/compilation.json", "w") as f:
        json.dump(ret, f, indent=4)


if __name__ == "__main__":
    get_full_statistic()
