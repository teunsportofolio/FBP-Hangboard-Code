import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.stats import linregress

MAIN_COLOR = "#bf1e2e"
LINE_COLOR = "black"

def load_and_clean_data(filepath, min_valid=2):
    df = pd.read_csv(filepath, header=None)
    df = df.apply(pd.to_numeric, errors='coerce')
    df = df.dropna(axis=1, how='all')
    df = df.loc[:, df.count() >= min_valid]
    nunique = df.nunique(dropna=True)
    df = df.loc[:, nunique > 1]
    return df

def compute_column_stats(df):
    stats = pd.DataFrame({
        'mean': df.mean(skipna=True),
        'std': df.std(skipna=True),
        'max': df.max(skipna=True)
    })
    return stats

def plot_scatter_with_trendline(x, y, data, xlabel, ylabel, title, fname):
    if data[x].nunique() < 2 or data[y].nunique() < 2:
        print(f"Not enough variation in {x} or {y} for correlation.")
        return

    slope, intercept, r_value, p_value, std_err = linregress(data[x], data[y])
    trendline = slope * data[x] + intercept

    plt.figure(figsize=(8,6))
    sns.scatterplot(x=x, y=y, data=data, label='Data', color=MAIN_COLOR, s=60)
    plt.plot(data[x], trendline, color=LINE_COLOR, linewidth=2, label=f'Trendline (r={r_value:.3f})')
    plt.title(f"{title}\nr = {r_value:.3f}")
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend()
    plt.tight_layout()
    plt.savefig(fname)
    plt.show()
    print(f"{title}: r = {r_value:.3f}")

def plot_and_save(stats, export_prefix="column_stats"):
    # Scatterplots with trendlines and r
    plot_scatter_with_trendline(
        'mean', 'std', stats,
        xlabel='Mean', ylabel='Standard Deviation',
        title='Standard Deviation vs Mean of Columns',
        fname=f"{export_prefix}_mean_vs_std.png"
    )
    plot_scatter_with_trendline(
        'mean', 'max', stats,
        xlabel='Mean', ylabel='Max',
        title='Max vs Mean of Columns',
        fname=f"{export_prefix}_mean_vs_max.png"
    )
    plot_scatter_with_trendline(
        'std', 'max', stats,
        xlabel='Standard Deviation', ylabel='Max',
        title='Max vs Standard Deviation of Columns',
        fname=f"{export_prefix}_std_vs_max.png"
    )

    # Pairplot for all relationships (main color applied to markers and diagonal hists)
    pair = sns.pairplot(
        stats,
        plot_kws={"color": MAIN_COLOR},
        diag_kind='hist',
        diag_sharey=False
    )
    # Recolor diagonal histograms
    for i, ax in enumerate(np.diag(pair.axes)):
        for patch in ax.patches:
            patch.set_facecolor(MAIN_COLOR)
    plt.suptitle("Pairplot of Column Statistics", y=1.02)
    plt.savefig(f"{export_prefix}_pairplot.png")
    plt.show()

    # Bar plots for mean, std, max with all column numbers on x-axis
    for stat, fname in zip(
        ['mean', 'std', 'max'],
        [f"{export_prefix}_mean_bar.png", f"{export_prefix}_std_bar.png", f"{export_prefix}_max_bar.png"]
    ):
        plt.figure(figsize=(max(10, int(len(stats)*0.4)), 4))
        ax = sns.barplot(x=np.arange(len(stats[stat])), y=stats[stat].values, color=MAIN_COLOR)
        plt.title(f'{stat.capitalize()} of Each Column')
        plt.xlabel('Column Index')
        plt.ylabel(stat.capitalize())
        plt.xticks(np.arange(len(stats[stat])), np.arange(len(stats[stat])))
        plt.tight_layout()
        plt.savefig(fname)
        plt.show()

if __name__ == "__main__":
    input_csv = r"C:\Users\T\exportedv5.csv"
    export_prefix = "column_stats"

    df = load_and_clean_data(input_csv)
    stats = compute_column_stats(df)
    plot_and_save(stats, export_prefix)