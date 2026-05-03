import pandas as pd
import matplotlib.pyplot as plt
import argparse
import seaborn as sns
import itertools
from tabulate import tabulate
import sys
from cycler import cycler
import numpy as np
import os
import math
from matplotlib.path import Path
from matplotlib.patches import PathPatch

import glob

# Define color schemes
# RED, BLUE, GREEN, ORANGE, PINK, CYAN, YELLOW, TURQUOISE
DARK_COLORS = [
    '#ff9999', '#66b3ff', '#99ff99', '#ffcc99', '#ff99cc', '#99ccff', '#ffff99', '#99ffcc',
    '#c299ff', '#ffb3b3', '#b3d9ff', '#b3ffb3', '#ffd9b3', '#ffb3d9', '#d9b3ff', '#b3fff0',
    '#e6b3b3', '#b3b3ff', '#d9ff99', '#ff99ff', '#99ffff', '#ffd699', '#b399ff', '#99ffcc'
]
LIGHT_COLORS = [
    '#ff3333', '#3366ff', '#33cc33', '#ff9933', '#ff3399', '#3399ff', '#cccc00', '#00cc99',
    '#9933ff', '#cc0000', '#0066cc', '#006600', '#cc6600', '#cc0066', '#6600cc', '#009999',
    '#663300', '#333399', '#669900', '#990066', '#006666', '#cc3300', '#330099', '#00994c'
]

DARK_THEME = {
    'bg_color': '#1C1C1C',
    'fg_color': 'white',
    'plot_bg_color': '#2F2F2F',
    'grid_color': '#555555',
}

LIGHT_THEME = {
    'bg_color': 'white',
    'fg_color': '#555555',
    'plot_bg_color': '#F0F0F0',
    'grid_color': '#CCCCCC',
}

# Global variables
timeout = 0
theme = LIGHT_THEME
colors = LIGHT_COLORS
output_dir = ''
output_format = 'pdf'

# Define result mappings for normalization
RESULT_MAPPING = {
    'SATISFIABLE': 'SATISFIABLE',
    'SAT': 'SATISFIABLE',
    'UNSATISFIABLE': 'UNSATISFIABLE',
    'UNSAT': 'UNSATISFIABLE'
}

def print_error(message):
    RED = "\033[91m"
    BOLD = "\033[1m"
    RESET = "\033[0m"
    print(f"{RED}{BOLD}{message}{RESET}")

def normalize_results(df):
    """
    Normalizes different result formats to consistent terminology.
    Converts 'SAT' to 'SATISFIABLE' and 'UNSAT' to 'UNSATISFIABLE' in all result columns.
    
    Parameters:
        df (pandas.DataFrame): DataFrame containing solver results
        
    Returns:
        pandas.DataFrame: Updated DataFrame with normalized result terminology
    """
    # Create a copy to avoid modifying the original dataframe
    updated_df = df.copy()
    
    # Get all result columns (those ending with _result)
    result_columns = [col for col in updated_df.columns if col.endswith('_result')]
    
    for result_col in result_columns:
        # Replace values according to mapping dictionary
        updated_df[result_col] = updated_df[result_col].map(lambda x: RESULT_MAPPING.get(x, x))
    
    return updated_df

def enforce_timeouts(df, timeout):
    """
    Tags results as 'TIMEOUT' for any solver whose time is >= timeout.
    Raw time values are preserved unchanged; the PAR2 penalty (2*timeout)
    is applied only when computing the PAR2 summary statistic.

    Parameters:
        df (pandas.DataFrame): DataFrame containing solver results
        timeout (float): Timeout threshold value

    Returns:
        pandas.DataFrame: Updated DataFrame with enforced timeout rules
    """
    updated_df = df.copy()

    solver_columns = [col for col in updated_df.columns if col.endswith('_time')]

    for solver_col in solver_columns:
        solver_name = solver_col[:-5]  # Remove '_time' suffix
        result_col = f"{solver_name}_result"
        timeout_mask = updated_df[solver_col] >= timeout
        updated_df.loc[timeout_mask, result_col] = 'TIMEOUT'

    return updated_df

def generate_csv_from_dirs(base_dir):
    # Find all solver directories
    solver_dirs = glob.glob(os.path.join(base_dir, "metric_*"))
    
    if not solver_dirs:
        raise ValueError(f"No solver directories found in {base_dir}")
    
    all_dfs = []
    all_instances = set()
    
    for solver_dir in solver_dirs:
        solver_name = os.path.basename(solver_dir).split('_', 1)[1]
        
        # Find the time CSV file
        time_files = glob.glob(os.path.join(solver_dir, "times_*.csv"))
        if not time_files:
            print_error(f"Warning: No time file found for solver {solver_name}")
            continue
        
        # Read the CSV file
        df = pd.read_csv(time_files[0])
        df = df.sort_values("instance")
        
        # Rename columns to match required format
        df = df.rename(columns={
            "time": f"{solver_name}_time",
            "result": f"{solver_name}_result"
        })

        # Tag timeouts in result column; raw time is preserved
        timeout_mask = df[f"{solver_name}_time"] >= timeout
        df.loc[timeout_mask, f"{solver_name}_result"] = "TIMEOUT"

        # Select only required columns
        df = df[["instance", f"{solver_name}_time", f"{solver_name}_result"]]

        all_dfs.append(df)
        all_instances.update(df["instance"])
    
    # Ensure all dataframes have the same instances
    all_instances = sorted(list(all_instances))
    for i, df in enumerate(all_dfs):
        missing_instances = set(all_instances) - set(df["instance"])
        if missing_instances:
            print_error(f"Warning: Solver {os.path.basename(solver_dirs[i]).split('_', 1)[1]} is missing {len(missing_instances)} instances")
            missing_df = pd.DataFrame({
                "instance": list(missing_instances),
                f"{os.path.basename(solver_dirs[i]).split('_', 1)[1]}_time": [timeout] * len(missing_instances),
                f"{os.path.basename(solver_dirs[i]).split('_', 1)[1]}_result": ["TIMEOUT"] * len(missing_instances)
            })
            all_dfs[i] = pd.concat([df, missing_df]).sort_values("instance").reset_index(drop=True)
    
    # Merge all dataframes
    final_df = all_dfs[0]
    for df in all_dfs[1:]:
        final_df = pd.merge(final_df, df, on="instance", how="outer")
    
    # Normalize SAT/UNSAT to SATISFIABLE/UNSATISFIABLE
    final_df = normalize_results(final_df)
    
    # Check for contradictory results
    result_columns = [col for col in final_df.columns if col.endswith("_result")]
    for _, row in final_df.iterrows():
        results = set(row[result_columns])
        if "SATISFIABLE" in results and "UNSATISFIABLE" in results:
            print_error(f"Warning: Contradictory results found for instance {row['instance']}")
    
    # Save the final CSV
    output_path = os.path.join(base_dir, "combined_results.csv")
    final_df.to_csv(output_path, index=False)
    print(f"Combined CSV saved to {output_path}")
    
    return output_path

def compute_vbs(row):
    solver_columns = [col for col in row.index if col.endswith('_time')]
    best_time = float('inf')
    best_result = 'TIMEOUT'
    for solver in solver_columns:
        solver_name = solver[:-5]  # Remove '_time' suffix
        time_value = float(row[solver])
        result = row[f'{solver_name}_result']
        if time_value < best_time:
            best_time = time_value
            best_result = result
    return pd.Series({'vbs_time': best_time, 'vbs_result': best_result})

def is_solved(time_value):
    return time_value < timeout

def compute_statistics(df):
    result_table = []
    solver_columns = [col for col in df.columns if col.endswith('_time')]
    vbs_capped = df["vbs_time"].clip(upper=timeout)
    for solver in solver_columns:
        solver_name = solver[:-5]  # Remove '_time' suffix
        raw = df[solver].astype(float)
        # SOLVED: instances with time strictly below timeout
        solved_instances = (raw < timeout).sum()
        # PAR2: mean time with 2*timeout penalty for non-solved instances
        par2 = np.where(raw < timeout, raw, 2 * timeout).mean()

        vbs_sat_mask = df[f'vbs_result'] == 'SATISFIABLE'
        vbs_unsat_mask = df[f'vbs_result'] == 'UNSATISFIABLE'
        # PAR2 for SAT instances only
        if vbs_sat_mask.sum() > 0:
            raw_sat = raw[vbs_sat_mask]
            par2_sat = np.where(raw_sat < timeout, raw_sat, 2 * timeout).mean()
        else:
            par2_sat = float('nan')

        # PAR2 for UNSAT instances only
        if vbs_unsat_mask.sum() > 0:
            raw_unsat = raw[vbs_unsat_mask]
            par2_unsat = np.where(raw_unsat < timeout, raw_unsat, 2 * timeout).mean()
        else:
            par2_unsat = float('nan')

        sat_count = (df[f'{solver_name}_result'] == 'SATISFIABLE').sum()
        unsat_count = (df[f'{solver_name}_result'] == 'UNSATISFIABLE').sum()
        # VBS participations: instances where solver time equals VBS time
        vbs_participations = ((raw == df["vbs_time"]) & (df["vbs_time"] < timeout)).sum()
        # SGM: shifted geometric mean of performance ratios vs VBS
        capped = raw.clip(upper=timeout)
        shift = 1.0
        ratios = (capped + shift) / (vbs_capped + shift)
        sgm = np.exp(np.mean(np.log(ratios)))

        # SGM for SAT instances only (using VBS result for consistency with PAR2_SAT)
        if vbs_sat_mask.sum() > 0:
            sat_ratios = ratios[vbs_sat_mask]
            sgm_sat = np.exp(np.mean(np.log(sat_ratios)))
        else:
            sgm_sat = float('nan')

        # SGM for UNSAT instances only (using VBS result for consistency with PAR2_UNSAT)
        if vbs_unsat_mask.sum() > 0:
            unsat_ratios = ratios[vbs_unsat_mask]
            sgm_unsat = np.exp(np.mean(np.log(unsat_ratios)))
        else:
            sgm_unsat = float('nan')

        result_table.append([solver_name, solved_instances, par2, par2_sat, par2_unsat, sat_count, unsat_count, vbs_participations, sgm, sgm_sat, sgm_unsat])
    return result_table

def plot_cumulative_data(df, result_filter=None, title_suffix="", solver_order=None):
    plt.style.use('default')
    fig, ax = plt.subplots(figsize=(12, 6), dpi=300)

    plt.rcParams['axes.prop_cycle'] = cycler(color=colors)

    markers = ['o', 'v', '^', 'd', 's', 'p', '*', 'h', 'H', '1', '2', '3', '<', '4', '>', '+', 'x', 'D', '|', '_', 'P', 'X', '8']
    marker_iter = itertools.cycle(markers)

    if solver_order is not None:
        solver_columns = [f"{name}_time" for name in solver_order if f"{name}_time" in df.columns]
    else:
        solver_columns = [col for col in df.columns if col.endswith('_time')]
    for column in solver_columns:
        solver_name = column[:-5]  # Remove '_time' suffix
        if solver_name == "vbs":
            continue
        if result_filter:
            mask = df[f'{solver_name}_result'] == result_filter
            sorted_data = df.loc[mask, column].sort_values().reset_index(drop=True)
        else:
            sorted_data = df[column].sort_values().reset_index(drop=True)

        # Keep only solved instances and use a proper 1-based count for Y-axis
        solved_data = sorted_data[sorted_data < timeout].reset_index(drop=True)
        y_values = range(1, len(solved_data) + 1)

        marker = next(marker_iter)
        ax.plot(solved_data, y_values, label=solver_name, marker=marker, markersize=4, linestyle='-')

    ax.set_ylim(0, None)
    ax.set_xlim(0, timeout)
    ax.set_ylabel("Number of resolved instances")
    ax.set_xlabel("Execution Time (s)")
    ax.legend(loc='upper left', bbox_to_anchor=(1.02, 1), borderaxespad=0,
              facecolor=theme['plot_bg_color'], edgecolor=theme['fg_color'], labelcolor=theme['fg_color'])
    ax.set_title(f"Cumulative Execution Time Graph{title_suffix}", color=theme['fg_color'])
    ax.grid(True, color=theme['grid_color'], linestyle='--', linewidth=0.5)

    fig.patch.set_facecolor(theme['bg_color'])
    ax.set_facecolor(theme['plot_bg_color'])
    ax.tick_params(colors=theme['fg_color'])
    ax.xaxis.label.set_color(theme['fg_color'])
    ax.yaxis.label.set_color(theme['fg_color'])
    ax.title.set_color(theme['fg_color'])

    plt.tight_layout()
    output_path = os.path.join(output_dir, f'cumulative_plot{title_suffix.replace(" ", "_")}.{output_format}')
    plt.savefig(output_path, bbox_inches='tight')
    plt.close()

def compute_cti(df, x_col, y_col, answer_filter=None):
    """
    Compute CTI (Comparative Time Index) for two solvers.
    CTI = sum(time_x - time_y) over instances where both solvers solved (time < timeout).
    CTI < 0: x-solver wins; CTI > 0: y-solver wins; CTI == 0: tie.
    Returns (cti_value, n_common_instances, winner_name).
    """
    mask = (df[x_col] < timeout) & (df[y_col] < timeout)
    if answer_filter is not None:
        mask &= (df['vbs_result'] == answer_filter)
    subset = df[mask]
    n = len(subset)
    if n == 0:
        return 0.0, 0, None
    cti = float((subset[x_col] - subset[y_col]).sum())
    solver_x = x_col[:-5]
    solver_y = y_col[:-5]
    if cti < 0:
        winner = solver_x
    elif cti > 0:
        winner = solver_y
    else:
        winner = None
    return cti, n, winner

def plot_scatter_with_timeout(df):
    sns.set_theme(palette="bright")

    solver_columns = [col for col in df.columns if col.endswith('_time') and col != 'vbs_time']
    solver_pairs = list(itertools.combinations(solver_columns, 2))

    color_map = {'UNSATISFIABLE': 'forestgreen', 'SATISFIABLE': 'goldenrod', 'unknown': 'crimson'}
    marker_map = {'UNSATISFIABLE': 'x', 'SATISFIABLE': '+', 'unknown': '.'}
    alpha_map = {'UNSATISFIABLE': 0.6, 'SATISFIABLE': 0.8, 'unknown': 0.1}

    for x_label, y_label in solver_pairs:
        fig1, ax1 = plt.subplots(figsize=(10, 10))
        fig2, ax2 = plt.subplots(figsize=(10, 10))
        axes = [ax1, ax2]
        answers = ['UNSATISFIABLE', 'SATISFIABLE']

        # Create a copy of the DataFrame and convert PAR2 columns to float
        pair_df = df[~((df[x_label] >= timeout) & (df[y_label] >= timeout))].copy()
        pair_df[x_label] = pair_df[x_label].astype(float)
        pair_df[y_label] = pair_df[y_label].astype(float)

        # Use vectorized operations instead of apply
        pair_df.loc[:, x_label] = np.where(pair_df[x_label] < timeout,
                                           pair_df[x_label],
                                           timeout + 0.05 * timeout)
        pair_df.loc[:, y_label] = np.where(pair_df[y_label] < timeout,
                                           pair_df[y_label],
                                           timeout + 0.05 * timeout)

        solver_name_one = x_label[:-5]  # Remove '_time' suffix
        solver_name_two = y_label[:-5]  # Remove '_time' suffix

        for idx, answer in enumerate(answers):
            ax = axes[idx]
            subset = pair_df[pair_df['vbs_result'] == answer]
            ax.scatter(subset[x_label], subset[y_label],
                       c=color_map[answer],
                       marker=marker_map[answer],
                       label=answer,
                       alpha=alpha_map[answer],
                       s=200,
                       linewidths=3)

            ax.axhline(y=timeout, color=theme['fg_color'], linestyle='--', alpha=0.5)
            ax.axvline(x=timeout, color=theme['fg_color'], linestyle='--', alpha=0.5)
            
            # Unit line (diagonal)
            ax.plot([0, timeout], [0, timeout], color=theme['fg_color'], linestyle='--', linewidth=1.5, alpha=0.7)

            # Area 10-20% below diagonal
            vertices_10to20below = [
                (0, 0.2*timeout),
                (0, 0.1*timeout),
                (timeout - 0.1*timeout, timeout),
                (timeout, timeout),
                (timeout - 0.2*timeout, timeout)
            ]
            codes_10to20below = [Path.MOVETO] + [Path.LINETO] * (len(vertices_10to20below) - 1)
            path_10to20below = Path(vertices_10to20below, codes_10to20below)
            patch_10to20below = PathPatch(path_10to20below, facecolor=colors[6], alpha=0.2, edgecolor='none')
            ax.add_patch(patch_10to20below)

            # Area ±10% around diagonal
            vertices_balanced = [
                (0, 0.1*timeout),
                (0, 0),
                (0.1*timeout, 0),
                (timeout, timeout - 0.1*timeout),
                (timeout, timeout),
                (timeout - 0.1*timeout, timeout)
            ]
            codes_balanced = [Path.MOVETO] + [Path.LINETO] * (len(vertices_balanced) - 1)
            path_balanced = Path(vertices_balanced, codes_balanced)
            patch_balanced = PathPatch(path_balanced, facecolor=colors[3], alpha=0.2, edgecolor='none')
            ax.add_patch(patch_balanced)

            # Area 10-20% above diagonal
            vertices_10to20above = [
                (0.1*timeout, 0),
                (0.2*timeout, 0),
                (timeout, timeout - 0.2*timeout),
                (timeout, timeout - 0.1*timeout)
            ]
            codes_10to20above = [Path.MOVETO] + [Path.LINETO] * (len(vertices_10to20above) - 1)
            path_10to20above = Path(vertices_10to20above, codes_10to20above)
            patch_10to20above = PathPatch(path_10to20above, facecolor=colors[6], alpha=0.2, edgecolor='none')
            ax.add_patch(patch_10to20above)

            ax.legend(loc='upper left', facecolor=theme['plot_bg_color'], edgecolor=theme['fg_color'], labelcolor=theme['fg_color'])
            ax.set_xlabel(f'{solver_name_one} (seconds)', fontsize=28)
            ax.set_ylabel(f'{solver_name_two} (seconds)', fontsize=28)
            ax.tick_params(axis='both', which='major', labelsize=20)
            ax.grid(True)

            fig1.patch.set_facecolor(theme['bg_color'])
            fig2.patch.set_facecolor(theme['bg_color'])
            ax.set_facecolor(theme['plot_bg_color'])
            ax.tick_params(colors=theme['fg_color'])
            ax.xaxis.label.set_color(theme['fg_color'])
            ax.yaxis.label.set_color(theme['fg_color'])
            ax.title.set_color(theme['fg_color'])

        # Compute and annotate CTI for each figure as a right-side legend panel
        for fig, ax, answer in [(fig1, ax1, 'UNSATISFIABLE'), (fig2, ax2, 'SATISFIABLE')]:
            cti, n_common, winner = compute_cti(df, x_label, y_label, answer_filter=answer)
            if n_common == 0:
                cti_lines = ["── CTI ──", "No commonly", "solved instances"]
            else:
                winner_str = winner if winner is not None else "tie"
                cti_lines = [
                    "── CTI ──",
                    f"Common: {n_common}",
                    f"Sum: {cti:+.2f} s",
                    f"Winner: {winner_str}",
                ]
            cti_text = "   ".join(cti_lines)
            fig.text(0.5, 1.01, cti_text, ha='center', va='bottom',
                     color=theme['fg_color'],
                     bbox=dict(boxstyle='round,pad=0.6', facecolor=theme['plot_bg_color'],
                               edgecolor=theme['grid_color'], alpha=0.9))

        fig1.savefig(os.path.join(output_dir, f'scatter_plot_{solver_name_one}_vs_{solver_name_two}_UNSAT.{output_format}'), bbox_inches='tight')
        fig2.savefig(os.path.join(output_dir, f'scatter_plot_{solver_name_one}_vs_{solver_name_two}_SAT.{output_format}'), bbox_inches='tight')
        plt.close(fig1)
        plt.close(fig2)

def main(input_path, scatter_plots, is_base_dir):
    global theme, colors, output_dir, output_format

    if is_base_dir:
        # Generate the combined CSV file only if base_dir is provided
        file_path = generate_csv_from_dirs(input_path)
    else:
        file_path = input_path

    try:
        df = pd.read_csv(file_path)
    except FileNotFoundError:
        print_error(f"Error: The file '{file_path}' was not found.")
        sys.exit(1)
    except pd.errors.EmptyDataError:
        print_error(f"Error: The file '{file_path}' is empty.")
        sys.exit(1)
    except pd.errors.ParserError:
        print_error(f"Error: Unable to parse '{file_path}'. Make sure it's a valid CSV file.")
        sys.exit(1)

    # Normalize different result formats (SAT/UNSAT)
    df = normalize_results(df)
    
    # Apply timeout enforcement
    df = enforce_timeouts(df, timeout)

    # Compute virtual best solver
    df[['vbs_time', 'vbs_result']] = df.apply(compute_vbs, axis=1)

    result_table = compute_statistics(df)

    result_df = pd.DataFrame(result_table, columns=['Solver', 'SOLVED', 'PAR2', 'PAR2_SAT', 'PAR2_UNSAT', 'SATISFIABLE', 'UNSATISFIABLE', 'VBS-Contribs', 'SGM', 'SGM_SAT', 'SGM_UNSAT'])
    result_df = result_df.sort_values(by='PAR2', ascending=True)

    print(tabulate(result_df, headers='keys', tablefmt='grid', showindex=False))

    summary_csv_path = os.path.join(output_dir, 'results_summary.csv')
    result_df.to_csv(summary_csv_path, index=False)
    print(f"Results summary saved to {summary_csv_path}")

    solver_order = result_df['Solver'].tolist()

    if scatter_plots:
        plot_scatter_with_timeout(df)
    else:
        plot_cumulative_data(df, title_suffix=" (All Instances)", solver_order=solver_order)
        plot_cumulative_data(df, result_filter='SATISFIABLE', title_suffix=" (SATISFIABLE Instances)", solver_order=solver_order)
        plot_cumulative_data(df, result_filter='UNSATISFIABLE', title_suffix=" (UNSATISFIABLE Instances)", solver_order=solver_order)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="""
    Compute solver statistics from CSV data and generate plots.
    
    Expected CSV column format:
    - For each solver, there should be two columns:
      1. <solver_name>_time: Contains the solving time for the solver
      2. <solver_name>_result: Contains the result (e.g., 'SATISFIABLE', 'UNSATISFIABLE', 'SAT', 'UNSAT')

    Example columns for a solver named 'solver1':
    solver1_time,solver1_result

    The script will automatically detect all solvers based on this naming convention.
    The --base-dir option can generate the needed csv file from the metric directories
    by using the --timeout option as a limit for the non-timeout time value.
    """)
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument("--base-dir", help="Path to the base directory containing metric subdirectories")
    input_group.add_argument("--file", help="Path to an existing CSV file")
    parser.add_argument("--timeout", type=int, required=True, help="Timeout threshold in seconds")
    parser.add_argument("--scatter-plots", action="store_true", help="Generate scatter plots instead of cumulative plots")
    parser.add_argument("--output-dir", default=".", help="Path to save directory")
    parser.add_argument("--output-format", default="pdf", help="Saving format for plots")
    parser.add_argument("--dark-mode", action="store_true", help="Use dark mode for plots")
    args = parser.parse_args()
    
    timeout = args.timeout
    theme = DARK_THEME if args.dark_mode else LIGHT_THEME
    colors = DARK_COLORS if args.dark_mode else LIGHT_COLORS
    output_dir = args.output_dir
    output_format = args.output_format

    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if args.base_dir:
        input_path = args.base_dir
        is_base_dir = True
    else:
        input_path = args.file
        is_base_dir = False

    main(input_path, args.scatter_plots, is_base_dir)
