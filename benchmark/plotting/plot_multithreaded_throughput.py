import pandas as pd
from utils import *
import matplotlib.pyplot as plt
import seaborn as sns

def sanity_check(df):
    group_cols = ['scaleFactor', 'table', 'pattern']

    # 1. Find groups where the match count isn't consistent
    # We filter for groups that have > 1 unique value in 'numMatches'
    inconsistent_groups = df.groupby(group_cols)['numMatches'].transform('nunique') > 1

    if not inconsistent_groups.any():
        print("✅ Sanity check passed: All matches are consistent.")
        return

    print("❌ SANITY CHECK FAILED: Found conflicting results.\n")

    # 2. Get the problematic data
    problem_df = df[inconsistent_groups]

    # 3. For each group, show only the distinct results found
    # This reduces hundreds of rows down to just the "candidates" that differ
    for keys, group in problem_df.groupby(group_cols):
        print(f"Group: scaleFactor={keys[0]}, table={keys[1]}, pattern={keys[2]}")

        # Drop duplicates based on numMatches to see the unique outcomes
        # We keep the first occurrence of each unique 'numMatches' value
        diffs = group.drop_duplicates(subset=['numMatches'])

        # Select key columns to show why they might differ (algo, threads, etc.)
        display_cols = ['numMatches', 'algorithm', 'numThreads', 'runNumber']
        print(diffs[display_cols].to_string(index=False))
        print("-" * 50)

def get_dataframe(csv_path: str):
    df = pd.read_csv(csv_path)
    sanity_check(df=df)
    pivot_keys = [
        'runNumber', 'scaleFactor', 'numThreads',
        'numLines', 'numBytes', 'table', 'pattern'
    ]
    df = df.pivot_table(
        index=pivot_keys,
        columns='algorithm',
        values=['runTime']
    ).reset_index()
    df['SIMDStringSearch'] = df['numBytes'] / (df[('runTime', 'SSEDecoded')] * 1e6)
    df['VectorScan'] = df['numBytes'] / (df[('runTime', 'VectorScan')] * 1e6)
    df['Interpreted'] = df['numBytes'] / (df[('runTime', 'InMemory')] * 1e6)
    df['LLVM Compiled'] = df['numBytes'] / (df[('runTime', 'SSELLVMCompiled')] * 1e6)
    df['C++ Compiled'] = df['numBytes'] / (df[('runTime', 'SSECppCompiled')] * 1e6)
    df["patternType"] = df["pattern"].apply(get_pattern_type)

    cols_to_drop = [
        ('runTime', 'InMemory'),
        ('runTime', 'SSEDecoded'),
        ('runTime', 'SSELLVMCompiled'),
        ('runTime', 'VectorScan'),
        ('runTime', 'SSECppCompiled'),
        'numLines', 'numBytes', 'scaleFactor', 'pattern', 'table'
    ]
    df = df.drop(columns=cols_to_drop)
    group_cols = [
        'numThreads', 'patternType'
    ]
    df = df.drop(columns='runNumber')
    df = df.groupby(group_cols).median().reset_index()
    df.columns = [col[1] if isinstance(col, tuple) and col[1] else col[0] for col in df.columns]
    return df

def plot(csv_path: str, out_path: str):
    df = get_dataframe(csv_path=csv_path)

    # sharey=True ensures the scale is identical across all pattern types
    latexify(fig_width=cm2inch(18), fig_height=cm2inch(4), columns=2)
    fig, axes = plt.subplots(1, 3, sharey=False)


    # 3. Loop through each pattern type and create a subplot
    for i, p_type in enumerate(get_all_pattern_types()):
        ax = axes[i]
        format_axes(ax)

        # Filter the data for the current pattern (e.g., 'prefix')
        subset = df[df['patternType'] == p_type]

        # Melt data so 'Approach' becomes a column, making it easy for Seaborn to color
        melted = subset.melt(
            id_vars=['numThreads'],
            value_vars=get_approaches(),
            var_name='Approach',
            value_name='Throughput'
        )

        # Create the line plot
        sns.lineplot(
            data=melted,
            x='numThreads',
            y='Throughput',
            hue='Approach',
            style='Approach',
            palette=get_approach_palette(),
            markers=get_approach_markers(),
            dashes=False,
            ax=ax
        )

        # 4. Styling and Labels
        ax.set_title(f"\\textbf{{{p_type.capitalize()}}}")
        ax.set_xlabel("\\# threads")

        ax.set_xticks([1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])
        ax.set_xticklabels([1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20])

        ax.axvspan(10, 20, color='gray', alpha=0.3, linewidth=0)
        ax.text(
            15, 0.42, '\\textbf{{HT}}',
            transform=ax.get_xaxis_transform(),
            ha='center', va='center',
            color='white', fontsize=8
        )

        if i == 0:
            ax.set_ylabel("Throughput (GB/s)")
        else:
            ax.set_ylabel("") # Hide Y label for middle/right plots for cleanliness

        ax.get_legend().remove()

    handles, labels = axes[0].get_legend_handles_labels()
    labels = [get_approach_labels(label) for label in labels]
    fig.legend(
        handles,
        labels,
        loc='lower center',
        ncol=len(labels),
        frameon=False,
        bbox_to_anchor=(0.5, -0.02),
    )

    plt.tight_layout()
    plt.savefig(out_path)

if __name__ == "__main__":
    csv_path = "../data/imdb_multithreaded.csv"
    out_path = "multithreaded_throughput_plot.pdf"
    plot(csv_path=csv_path, out_path=out_path)