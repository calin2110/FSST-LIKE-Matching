import numpy as np
import matplotlib
from matplotlib.ticker import AutoMinorLocator

def get_pattern_type(pattern: str) -> str:
    if pattern[0] != '%':
        return 'prefix'
    if pattern[-1] != '%':
        return 'suffix'
    return 'substring'

def get_all_pattern_types():
    start = get_pattern_type('ABC%')
    middle = get_pattern_type('%ABC%')
    end = get_pattern_type('%ABC')
    return [start, middle, end]

def get_approach_palette():
    return {"Interpreted": "#4daf4a", "C++ Compiled": "#e41a1c", "LLVM Compiled": "#377eb8", "VectorScan": "#984ea3", "SIMDStringSearch": "#ff7f00"}

def get_approach_markers():
    return {'SIMDStringSearch': '^', 'Interpreted': 'o', 'LLVM Compiled': 's', 'VectorScan': 'D', 'C++ Compiled': 'v'}

def get_approaches():
    return ['SIMDStringSearch', 'VectorScan', 'Interpreted', 'C++ Compiled', 'LLVM Compiled']

def get_datasets():
    return ['TPCH', 'StackOverflow', 'IMDB', 'PublicBI']

def get_dataset_title(dataset):
    dataset_titles = {'TPCH': 'TPC-H', 'StackOverflow': 'Stack Overflow', 'IMDB': 'IMDB', 'PublicBI': 'Public BI'}
    return dataset_titles[dataset]

def get_approach_labels(approach, short_version=False):
    interpreted_val = "Interp." if short_version else "Interpreted"
    label_mapping = {'Interpreted': interpreted_val, 'C++ Compiled': 'C++', 'LLVM Compiled': 'LLVM',
                     'SIMDStringSearch': 'HSS', 'VectorScan': 'VS'}
    return label_mapping[approach]

def get_phases():
    return ['Preparation', 'Decoding', 'Matching']

def get_phase_colour(phase):
    palette = {'Preparation': '#7570b3', 'Decoding': '#1b9e77', 'Matching': '#d95f02'}
    return palette[phase]

def format_size(num_bytes, precision=1):
    """Returns a string scaling from B to TB based on size."""
    val = float(num_bytes)
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if abs(val) < 1024:
            # Use :g to avoid .00 for whole numbers, or fixed precision
            return f"{val:.{precision}f} {unit}" if val % 1 != 0 else f"{int(val)}{unit}"
        val /= 1024.0
    return f"{val:.{precision}f}PB"


def format_tuplecount(num_lines):
    """Prints number of lines with suffixes (K, M, B)."""
    for unit in ['', 'K', 'M', 'B', 'T']:
        if abs(num_lines) < 1000:
            # Print with one decimal if it's not a whole number, else integer
            format_str = "{:.1f}{}" if num_lines % 1 != 0 else "{:g}{}"
            return format_str.format(num_lines, unit)
        num_lines /= 1000.0
    return f"{num_lines:.1f}P"

def cm2inch(value):
    return value / 2.54


def latexify(fig_width=None, fig_height=None, columns=1):
    assert columns in [1, 2]
    if fig_width is None:
        fig_width = 3.39 if columns == 1 else 6.9
    if fig_height is None:
        golden_mean = (np.sqrt(5) - 1.0) / 2.0
        fig_height = fig_width * golden_mean
    params = {
        'axes.labelsize': 7,
        'axes.titlesize': 7,
        'font.size': 7,
        'legend.fontsize': 7,
        'legend.handlelength': 1.5,
        'legend.handletextpad': 0.2,
        'legend.labelspacing': 0.5,
        'legend.columnspacing': 0.7,
        'legend.borderpad': 0,
        'xtick.labelsize': 7,
        'ytick.labelsize': 7,
        'axes.labelpad': 0,
        'axes.titlepad': 0,
        'text.usetex': True,
        'font.family': 'serif',
        'text.latex.preamble': r'\usepackage{amssymb}',
        'pgf.rcfonts': False,
        'figure.figsize': [fig_width, fig_height],
    }
    matplotlib.rcParams.update(params)


def format_axes(ax):
    for spine in ['top', 'right']:
        ax.spines[spine].set_visible(False)
    for spine in ['left', 'bottom']:
        ax.spines[spine].set_color('black')
        ax.spines[spine].set_linewidth(0.5)
    ax.xaxis.set_ticks_position('bottom')
    ax.yaxis.set_ticks_position('left')
    for axis in [ax.xaxis, ax.yaxis]:
        axis.set_tick_params(direction='out', color='black')
    ax.yaxis.set_minor_locator(AutoMinorLocator(n=2))
    ax.yaxis.grid(True)
    ax.yaxis.grid(visible=True, which='minor', linestyle=':')
    ax.set_axisbelow(True)
    ax.tick_params(axis='both', which='major', pad=0.5)
    return ax
