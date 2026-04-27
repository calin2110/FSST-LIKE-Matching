import pandas as pd
import matplotlib.pyplot as plt
from utils import *
from matplotlib.patches import Patch

def get_dataframe(time_csv, patterns_csv, decoding_csv, dataset, scale_factors, table, column):
    pattern_df = pd.read_csv(patterns_csv)
    if column:
        pattern_df = pattern_df[
            (pattern_df['dataset'] == dataset) & (pattern_df['table'] == table) & (pattern_df['column'] == column)]
        pattern_cols_to_drop = ['dataset', 'queryNum', 'column', 'table']
    else:
        pattern_df = pattern_df[(pattern_df['dataset'] == dataset) & (pattern_df['table'] == table)]
        pattern_cols_to_drop = ['dataset', 'queryNum', 'table']
    pattern_df = pattern_df.drop(columns=pattern_cols_to_drop)
    time_df = pd.read_csv(time_csv)
    time_df = time_df[time_df['scaleFactor'].isin(scale_factors)]
    time_cols_to_drop = [
        'HSSCreate', 'HSSRun',
        'NoSSECppCompile', 'NoSSECppRun',
        'NoSSELLVMCompile', 'NoSSELLVMRun',
    ]
    time_df = time_df.drop(columns=time_cols_to_drop)
    df = pd.merge(pattern_df, time_df, on='patternId', how='inner')
    df = df.drop(columns=['patternId'])
    df = df.groupby(['pattern', 'scaleFactor']).median().reset_index()
    df = df.drop(columns=['runNumber'])
    df["patternType"] = df["pattern"].apply(get_pattern_type)
    df = df.groupby(['scaleFactor', 'patternType']).mean(numeric_only=True).reset_index()

    decoding_df = pd.read_csv(decoding_csv)
    if column:
        decoding_df = decoding_df[(decoding_df["dataset"] == dataset) & (decoding_df["column"] == column) & (
                decoding_df["table"] == table) & (decoding_df['scaleFactor'].isin(scale_factors))]
    else:
        decoding_df = decoding_df[(decoding_df["dataset"] == dataset) & (decoding_df["table"] == table) & (
            decoding_df['scaleFactor'].isin(scale_factors))]
    decoding_cols_to_drop = ["dataset", "schema", "table", "column", "count"]
    decoding_df = decoding_df.drop(columns=decoding_cols_to_drop)
    decoding_df = decoding_df.groupby(['scaleFactor']).mean().reset_index()
    decoding_df = decoding_df.drop(columns=['runNumber'])
    df = pd.merge(df, decoding_df, on='scaleFactor', how='inner')
    return df

def plot(time_csv, patterns_csv, decoding_csv, dataset, scale_factors, table, column, out_path):
    df = get_dataframe(time_csv=time_csv, patterns_csv=patterns_csv, decoding_csv=decoding_csv, dataset=dataset, scale_factors=scale_factors, table=table, column=column)

    scale_factors = sorted(df['scaleFactor'].unique())
    latexify(cm2inch(18), cm2inch(6))

    def get_vals(row, approach):
        match approach:
            case "SIMDStringSearch":
                return row['SSECreate'], row['SSERun'] - row['decodeTime'], row['decodeTime']
            case "VectorScan":
                return row['VectorScanCompile'], row['VectorScanRun'] - row['decodeTime'], row['decodeTime']
            case "Interpreted":
                return row['AutomatonCreate'], row['InterpretedRun'], 0
            case 'C++ Compiled':
                return row['AutomatonCreate'] + row['SSECppCompile'], row['SSECppRun'], 0
            case 'LLVM Compiled':
                return row['AutomatonCreate'] + row['SSELLVMCompile'], row['SSELLVMRun'], 0

    ncols = len(scale_factors)
    fig = plt.figure()
    gs = fig.add_gridspec(2, ncols)

    ax_top = fig.add_subplot(gs[0, 0])
    ax_bot = fig.add_subplot(gs[1, 0], sharex=ax_top)

    ax_top_mid = fig.add_subplot(gs[0, 1], sharey=ax_top)
    ax_bot_mid = fig.add_subplot(gs[1, 1], sharex=ax_top_mid)

    other_axes = []
    for col in range(2, ncols):
        if other_axes:
            ax = fig.add_subplot(gs[:, col], sharey=other_axes[0])
        else:
            ax = fig.add_subplot(gs[:, col])
        other_axes.append(ax)

    n_max_approaches = len(get_approaches())
    group_width = 0.8
    bar_slot = group_width / n_max_approaches
    bar_width = bar_slot * 0.85

    for idx, sf in enumerate(scale_factors):
        if idx == 0:
            bar_axes = [ax_top, ax_bot]
            bottom_ax = ax_bot
            title_ax = ax_top
        elif idx == 1:
            bar_axes = [ax_top_mid, ax_bot_mid]
            bottom_ax = ax_bot_mid
            title_ax = ax_top_mid
        else:
            ax = other_axes[idx - 2]
            bar_axes = [ax]
            bottom_ax = ax
            title_ax = ax

        sf_df = df[df['scaleFactor'] == sf].set_index('patternType').reindex(get_all_pattern_types())
        assert (sf_df['numBytes'] == sf_df['numBytes'].iloc[0]).all(), "Not all numBytes values are identical!"
        compressed_num_bytes = sf_df['numBytes'].iloc[0]
        uncompressed_num_bytes = sf * 1024 * 1024 * 1024

        assert (sf_df['numLines'] == sf_df['numLines'].iloc[0]).all(), "Not all numLines values are identical"
        num_lines = sf_df['numLines'].iloc[0]
        tuplecount_str = format_tuplecount(num_lines)
        bar_x_positions = []
        bar_x_labels = []
        for p_idx, p in enumerate(get_all_pattern_types()):
            # Filter approaches: remove 'Interpreted' if we are in the 'middle' pattern
            active_approaches = [a for a in get_approaches() if not (a == 'Interpreted' and p == 'substring')]

            n_bars = len(active_approaches)
            group_offset = (np.arange(n_bars) - (n_bars - 1) / 2) * bar_slot

            for a_idx, approach in enumerate(active_approaches):
                x_pos = p_idx + group_offset[a_idx]

                vals = get_vals(sf_df.loc[p], approach)
                prepare, match, decode = vals
                approach_key = get_approach_labels(approach, short_version=True)

                for axx in bar_axes:
                    axx.bar(x_pos, prepare, bar_width, color=get_phase_colour('Preparation'))
                    axx.bar(x_pos, decode, bar_width, bottom=prepare, color=get_phase_colour('Decoding'))
                    axx.bar(x_pos, match, bar_width, bottom=prepare + decode, color=get_phase_colour('Matching'))

                bar_x_positions.append(x_pos)
                bar_x_labels.append(approach_key)

        bottom_ax.set_xticks(bar_x_positions)
        bottom_ax.set_xticklabels(bar_x_labels, rotation=90, fontweight='bold')

        for p_idx, p in enumerate(get_all_pattern_types()):
            title_ax.text(p_idx, 1, p.capitalize(),
                          transform=title_ax.get_xaxis_transform(),
                          ha='center', va='bottom',
                          fontweight='bold')

        title_ax.set_title(
            f"\\textbf{{Sample of {tuplecount_str} tuples}}\n{{\\tiny({format_size(uncompressed_num_bytes, 0)} uncompressed)}}", pad=15
        )

        if idx == 0:
            format_axes(ax_top)
            format_axes(ax_bot)
            ax_top.set_ylim(290, 360)
            ax_bot.set_ylim(0, 35)
            # Hide bottom spine of top half to indicate the break
            ax_top.spines['bottom'].set_visible(False)
            ax_top.tick_params(axis='x', which='both', length=0, bottom=False, labelbottom=False)
            ax_bot.tick_params(axis='x', which='both', length=0, pad=2)
            # Diagonal break markers
            d = .015
            kwargs = dict(transform=ax_top.transAxes, color='k', clip_on=False, linewidth=0.5)
            ax_top.plot((-d, +d), (-d, +d), **kwargs)
            kwargs.update(transform=ax_bot.transAxes)
            ax_bot.plot((-d, +d), (1 - d, 1 + d), **kwargs)
            ax_bot.set_ylabel('Time (ms)', fontweight='bold')
            ax_bot.tick_params(axis='y', labelsize=7)
            ax_top.tick_params(axis='y', labelsize=7)
        elif idx == 1:
            format_axes(ax_top_mid)
            format_axes(ax_bot_mid)
            ax_top_mid.set_ylim(290, 360)
            ax_bot_mid.set_ylim(0, 90)
            # Hide bottom spine of top half to indicate the break
            ax_top_mid.spines['bottom'].set_visible(False)
            ax_top_mid.tick_params(axis='x', which='both', length=0, bottom=False, labelbottom=False)
            ax_bot_mid.tick_params(axis='x', which='both', length=0, pad=2)
            # Diagonal break markers
            d = .015
            kwargs = dict(transform=ax_top_mid.transAxes, color='k', clip_on=False, linewidth=0.5)
            ax_top_mid.plot((-d, +d), (-d, +d), **kwargs)
            # ax_top_mid.plot((1 - d, 1 + d), (-d, +d), **kwargs)
            kwargs.update(transform=ax_bot_mid.transAxes)
            ax_bot_mid.plot((-d, +d), (1 - d, 1 + d), **kwargs)
            # ax_bot_mid.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)
            ax_bot_mid.tick_params(axis='y', labelsize=7)
            ax_top_mid.tick_params(axis='y', labelsize=7)
        else:
            format_axes(ax)
            ax.tick_params(axis='x', which='both', length=0, pad=2)
            ax.tick_params(axis='y', labelsize=7)


    legend_elements = [Patch(facecolor=get_phase_colour(phase), label=phase) for phase in get_phases()]

    fig.legend(
        handles=legend_elements,
        loc='lower center',
        ncol=3,
        frameon=False,
        bbox_to_anchor=(0.523, -0.016),
    )

    # plt.subplots_adjust(bottom=0.25)
    plt.tight_layout()

    # Center the "Time (ms)" y-label vertically between the broken-axis halves.
    # Force a draw so the label has a real (post-tick-layout) x position before we override y.
    fig.canvas.draw()
    top_pos = ax_top.get_position()
    bot_pos = ax_bot.get_position()
    center_fig_y = (top_pos.y1 + bot_pos.y0) / 2
    center_axes_y = (center_fig_y - bot_pos.y0) / (bot_pos.y1 - bot_pos.y0)
    label_bbox = ax_bot.yaxis.label.get_window_extent()
    label_x_axes = ax_bot.transAxes.inverted().transform((label_bbox.x0, 0))[0]
    ax_bot.yaxis.set_label_coords(label_x_axes, center_axes_y)

    plt.savefig(out_path)

if __name__ == "__main__":
    time_csv = "../data/time.csv"
    patterns_csv = "../data/patterns.csv"
    decoding_csv = "../data/decoding_overhead.csv"

    dataset = "IMDB"
    scale_factors = [0.01, 0.05, 0.2]
    table = "quotes"
    column = None
    out_path = "runtime_plot.pdf"
    plot(time_csv=time_csv, patterns_csv=patterns_csv, decoding_csv=decoding_csv, dataset=dataset,
                         scale_factors=scale_factors, table=table, column=column, out_path=out_path)
