import pandas as pd
import matplotlib.pyplot as plt
from utils import *
import seaborn as sns
import matplotlib.patches as mpatches

def get_dataframe(patterns_csv, time_csv):
    pattern_df = pd.read_csv(patterns_csv)
    time_df = pd.read_csv(time_csv)
    time_df['SSEThroughput'] = time_df['numBytes'] / time_df['SSERun']
    time_df['InterpretedThroughput'] = time_df['numBytes'] / time_df['InterpretedRun']
    time_df['SSECppThroughput'] = time_df['numBytes'] / time_df['SSECppRun']
    time_df['SSELLVMThroughput'] = time_df['numBytes'] / time_df['SSELLVMRun']
    time_df['VectorScanThroughput'] = time_df['numBytes'] / time_df['VectorScanRun']

    cols_to_drop = [
        'numLines', 'numBytes',
        'HSSCreate', 'HSSRun',
        'SSECreate', 'SSERun',
        'AutomatonCreate', 'InterpretedRun',
        'NoSSECppCompile', 'NoSSECppRun',
        'SSECppCompile', 'SSECppRun',
        'NoSSELLVMCompile', 'NoSSELLVMRun',
        'SSELLVMCompile', 'SSELLVMRun',
        'VectorScanCompile', 'VectorScanRun'
    ]
    time_df = time_df.drop(columns=cols_to_drop)

    time_df = time_df.groupby(['patternId', 'scaleFactor']).median().reset_index()
    time_df = time_df.drop(columns=["runNumber"])
    time_df = time_df.groupby(['patternId']).mean().reset_index()
    time_df = time_df.drop(columns=['scaleFactor'])
    unique_datasets = pattern_df['dataset'].unique().tolist()

    patterns = {}
    for pattern_type in get_all_pattern_types():
        patterns[pattern_type] = {}
    for dataset in unique_datasets:
        for key in patterns:
            patterns[key][dataset] = []

    for row in pattern_df.itertuples(index=False):
        type = get_pattern_type(row.pattern)
        patterns[type][row.dataset].append(row.pattern)
    df = pd.merge(pattern_df, time_df, on='patternId', how='inner')
    df = df.drop(columns=['patternId', 'queryNum', 'table'])

    result = {}
    for pattern_type in get_all_pattern_types():
        result[pattern_type] = {}
    for dataset in unique_datasets:
        for key in patterns:
            result[key][dataset] = []

    for row in df.itertuples(index=False):
        dataset = row.dataset
        pattern_type = get_pattern_type(row.pattern)
        simd_string_search = row.SSEThroughput
        interpreted = row.InterpretedThroughput
        cpp = row.SSECppThroughput
        llvm = row.SSELLVMThroughput
        vectorscan = row.VectorScanThroughput
        result[pattern_type][dataset].append({
            "Interpreted": interpreted,
            "C++ Compiled": cpp,
            "LLVM Compiled": llvm,
            "VectorScan": vectorscan,
            "SIMDStringSearch": simd_string_search
        })

    rows = []
    for pattern_type, datasets in result.items():
        for dataset, measurements in datasets.items():
            for entry in measurements:
                # Direct mapping without checks as data is guaranteed clean
                for approach in get_approaches():
                    rows.append({
                        'Pattern': pattern_type,
                        'Dataset': dataset,
                        'Approach': approach,
                        'Throughput': float('nan') if pattern_type == 'substring' and approach == 'Interpreted' else (
                                entry[approach] / 1e6)
                    })

    return pd.DataFrame(rows)


def plot(patterns_csv, time_csv, out_path):
    df = get_dataframe(patterns_csv=patterns_csv, time_csv=time_csv)

    sns.set_theme(style="whitegrid")
    latexify(cm2inch(18), cm2inch(8))

    # Create 3x4 grid
    fig, axes = plt.subplots(3, 4, sharex=False)

    for row_idx, pattern in enumerate(get_all_pattern_types()):
        for col_idx, dset in enumerate(get_datasets()):
            ax = axes[row_idx, col_idx]

            # Filter data for this specific cell
            plot_df = df[(df['Dataset'] == dset) & (df['Pattern'] == pattern)]

            sns.boxplot(
                data=plot_df,
                x='Approach',
                y='Throughput',
                hue='Approach',
                hue_order=get_approaches(),
                palette=get_approach_palette(),
                ax=ax,
                showfliers=False,
                linewidth=1.0,
                fliersize=1.5
            )
            format_axes(ax)
            ax.set_xticks([])
            if row_idx == 0:
                ax.set_title(f"\\textbf{{{get_dataset_title(dset)}}}", fontweight='bold')
            else:
                ax.set_title("")

            if col_idx == 0:
                ax.set_ylabel(f"\\textbf{{{pattern.capitalize()}}}\n{{\\scriptsize Throughput(GB/s)}}", fontweight='bold')
            else:
                ax.set_ylabel("")

            ax.set_xlabel("")
            ax.set_ylim(bottom=0)
            ax.tick_params(axis='both', labelsize=7)
            if ax.get_legend():
                ax.get_legend().remove()

    # Custom Legend at the bottom
    legend_handles = [
        mpatches.Patch(color=get_approach_palette()[key], label=get_approach_labels(key))
        for key in get_approaches()
    ]
    labels = [get_approach_labels(key) for key in get_approaches()]

    fig.legend(
        handles=legend_handles,
        labels=labels,
        loc='lower center',
        ncol=5,
        frameon=False,
        bbox_to_anchor=(0.5, -0.015)
    )

    fig.align_ylabels(axes[:, 0])
    plt.tight_layout()
    plt.savefig(out_path)

if __name__ == "__main__":
    time_csv = "../data/time.csv"
    patterns_csv = "../data/patterns.csv"
    out_path = "singlethreaded_throughput_plot.pdf"
    plot(time_csv=time_csv, patterns_csv=patterns_csv, out_path=out_path)
