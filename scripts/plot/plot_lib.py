
import itertools
import math

import statistics as stats

from matplotlib import rc
from matplotlib import pyplot as plt

def plot_square(df,
                xaxis='runtime_solve',
                yaxis='runtime_check',
                xlabel='Solving time in s',
                ylabel='Checking time in s',
                title='X Nodes',
                mark='.',
                filename=None):
    """
    Plots a square comparison between xaxis and yaxis.
    """
    fig = plt.figure()
    ax = fig.add_subplot()
    ax.plot([0, 1], [0, 1], transform=ax.transAxes, color='grey')
    plt.plot(df[xaxis],
             df[yaxis],
             mark)
    _min = min(min(df[xaxis]),
              min(df[yaxis]))
    _max = max(max(df[xaxis]),
              max(df[yaxis]))
    plt.axis([_min, _max] * 2)
    plt.xscale('log')
    plt.yscale('log')
    ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.grid()
    if filename:
        plt.savefig(filename, bbox_inches='tight')

def plot_CDF(dfs, labels,
                          column_to_plot='runtime_solve',
                          figsize=[5.5, 2.75],
                          line_styles=['-', '--'],
                          colors=['tab:blue', 'tab:blue', 'tab:orange', 'tab:orange'],
                          xlim=None,
                          ylim=None,
                          show=False,
                          legend_spacing=0,
                          square=True,
                          filename=None):
    if len(dfs) != len(labels):
        raise ValueError("dfs and labels need to be of same length")
    line_style = itertools.cycle(line_styles)
    color = itertools.cycle(colors)
    rc('text', usetex=True)
    rc('font', family='serif')

    # get xmax befor plots
    xmax = max([ max(df[df[column_to_plot].notnull()][column_to_plot]) for df in dfs ])
    ymax = max([ len(df[df[column_to_plot].notnull()]) - 1 for df in dfs ])
    
    if xlim: xmax = xlim
    if ylim: ymax = ylim

    fig = plt.figure(figsize=figsize)
    ax = fig.add_subplot()

    for i in range(0, len(dfs)):
        df = dfs[i].sort_values(column_to_plot)
        df = df[df[column_to_plot].notnull()]
        df = df[df[column_to_plot] <= xmax]
        label = labels[i]

        plt.plot(list(df[column_to_plot]) + [xmax],
                 list(range(0, len(df))) + [len(df) - 1],
                 next(line_style),
                 label=label,
                 color=next(color))
        
    
    ax.axis([0, xmax, 0, ymax])
    if square:
        ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    
    plt.xlabel('Runtime $t$ in s')
    plt.ylabel('\# solved instances in $\leq t$')
    plt.xticks(range(0, math.ceil(xmax+1), 60))
    plt.grid()
    plt.legend(labelspacing=legend_spacing)
    plt.tight_layout()
    if filename:
        plt.savefig(filename, bbox_inches='tight')
    if show:
        plt.show()

def plot_tight_square(dfs,
                      labels='',
                      xaxis='runtime_solve',
                      xlabel='Solving time in s',
                      ylabel='Checking time in s',
                      marks=['+', '.', 'x', '*'],
                      colors=['blue', 'orange', 'green', 'red'],
                      title='',
                      figsize=[5.5, 2.75],
                      legend_spacing=0,
                      show=False,
                      filename=None):
    if len(dfs) != len(labels):
        raise ValueError("dfs and labels must have same length")
    
    mark_style = itertools.cycle(marks)
    colors = itertools.cycle(colors)
    rc('text', usetex=True)
    rc('font', family='serif')

    _min = 3000
    _max = 0

    fig = plt.figure(figsize=figsize)
    ax = fig.add_subplot()
    ax.plot([0, 1], [0, 1], transform=ax.transAxes, color='grey')

    for df, label in zip(dfs, labels):
        check_time = [ x+y+z+a+b+c for x,y,z,a,b,c in zip(df["max_runtime_first_pass"],
                                                          df["max_runtime_reroute"],
                                                          df["max_runtime_last_pass"],
                                                          df["max_waittime_first_pass"],
                                                          df["max_waittime_reroute"],
                                                          df["max_waittime_last_pass"]) ] if "max_runtime_first_pass" in df else df['runtime_check']

        plt.plot(df[xaxis],
                 check_time,
                 next(mark_style),
                 label=label,
                 color=next(colors),
                 markersize=5)
        _min = min(min(df[xaxis]),
                  min(check_time), _min)
        _max = max(max(df[xaxis]),
                  max(check_time), _max)
    
    #ax.axis([_min, _max] * 2)
    ax.axis([_min, 300] * 2)
    plt.xscale('log')
    plt.yscale('log')
    ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend(loc='center left', bbox_to_anchor=(1, 0.5), labelspacing=legend_spacing)
    plt.grid()
    if filename:
        plt.savefig(filename, bbox_inches='tight')
    if show:
        plt.show()

def plot_checker_components_runtime(dfs, titles,
                                    marks=['+', '.', 'x', '*'],
                                    overall_col='runtime_check',
                                    comp_cols=['runtime_lc',
                                               'runtime_redist',
                                               'runtime_con'],
                                    labels=['Local checking',
                                            'Redistribution',
                                            'Confirmation'],
                                    colors=['tab:blue', 'tab:red', 'tab:green', 'tab:cyan'],
                                    figsize=[5.5, 2.75],
                                    title='',
                                    legend_spacing=0,
                                    show=False,
                                    filename=None):
    if len(dfs) != len(titles):
        raise ValueError("dfs and titles must have same length")
    if len(comp_cols) != len(labels):
        raise ValueError("comp_cols and labels must have same length")

    mark_style = itertools.cycle(marks)
    color = itertools.cycle(colors)
    rc('text', usetex=True)
    rc('font', family='serif')

    fig, axs = plt.subplots(1, len(dfs), figsize=figsize)
    
    for i in range(0, len(dfs)):
        df = dfs[i].sort_values(overall_col)
        df = df[df[overall_col].notnull()]
        title = titles[i]
        if len(dfs) > 1: ax = axs[i]
        else: ax = axs
        xrange = range(0, len(df))

        ax.plot(xrange,
                df[overall_col],
                next(mark_style),
                label='Overall Runtime',
                markersize=3,
                color=next(color))
        for (col, label) in zip(comp_cols, labels):
            ax.plot(xrange,
                     df[col],
                     next(mark_style),
                     label=label,
                     markersize=3,
                     color=next(color))

        if i == 0:
            ax.set(ylabel='Runtime in s')
        ax.set(title=title, yscale='log', xlim=[0, len(df)], ylim=[0.03,300])
        ax.grid(axis='y')
        ax.tick_params(axis='x', which='both', bottom=False, top=False, labelbottom=False)
        ax.set_box_aspect(1)

    fig.legend(labels=['WC runtime']+labels, loc='center left', bbox_to_anchor=(1, 0.5), handlelength=.5, labelspacing=legend_spacing)
    # only ceneters xlabel correctly for odd number of instances, whis is fine for our usecase
    if len(dfs) > 1: ax = axs[int(len(dfs) / 2)]
    else: ax = axs.set(xlabel='Instances sorted by Runtime')
    ax.set(xlabel='Instances sorted by Runtime')
    fig.tight_layout()
    if filename:
       fig.savefig(filename, bbox_inches='tight')
    if show:
        plt.show()

def plot_boxplots(dfs, columns_to_plot, labels,
                  titles=[],
                  figsize=[5.5, 2.75],
                  show=False,
                  filename=None):
    if len(columns_to_plot) != len(labels):
        raise ValueError("columns_to_plot needs to be of same length as labes")
    
    rc('text', usetex=True)
    rc('font', family='serif')

    fig, axs = plt.subplots(1, len(dfs), figsize=figsize)
    titles += [''] * len(dfs)

    for i in range(0, len(dfs)):
        df = dfs[i]
        if len(dfs) > 1: ax = axs[i]
        else: ax = axs

        data = []
        for column in columns_to_plot:
            data.append(df[df[column] > 0][column])

        ax.boxplot(data,
                   labels=labels,
                   positions=[1/4, 2/4, 3/4])
        #ax.yticks(ticks=[0,0.2,0.4,0.6,0.8,1],
        #           labels=['0','0.2','0.4','0.6','0.8','1'])
        ax.set_xticklabels(labels, rotation=40, ha='right')
        ax.set(title=titles[i], xlim=[0, 1], ylim=[0,1])
        ax.grid(axis='y')
        ax.set_box_aspect(1)
    
    #plt.xticks(rotation=45, ha='right')
    fig.tight_layout()
    if filename:
        fig.savefig(filename, bbox_inches='tight')
    if show:
        plt.show()

def plot_scatter(dfs, labels,
                 xaxis = 'runtime_solve',
                 yaxis = 'proof_bytes',
                 xlabel='Solving time in s',
                 ylabel='Proof size in bytes',
                 marks=['+', '.', 'x', '*'],
                 colors=['blue', 'orange', 'green', 'red'],
                 title='',
                 figsize=[5.5, 2.75],
                 show=False,
                 xscale='log',
                 xscale_base=10,
                 yscale='log',
                 yscale_base=10,
                 xlim=None,
                 ylim=None,
                 square=True,
                 legend_spacing=0,
                 xticks=None,
                 yticks=None,
                 minor_ticks=False,
                 plot_median=False,
                 filename=None):
    if len(dfs) != len(labels):
        raise ValueError("labes has to be oof same length as dfs")
    
    mark_style = itertools.cycle(marks)
    colors = itertools.cycle(colors)
    rc('text', usetex=True)
    rc('font', family='serif')
    fig = plt.figure(figsize=figsize)
    ax = fig.add_subplot()

    for i in range(0, len(dfs)):
        df = dfs[i]
        df = df[(df[xaxis].notnull()) & (df[yaxis].notnull())]
        color = next(colors)

        if plot_median:
            median = stats.median([ y / x for x,y in zip(df[xaxis], df[yaxis]) ])
            print(f"Median for {labels[i]}:",
                  median, 'B,',
                  median / 1024, 'KiB,',
                  median / (1024**2), 'MiB,',
                  median / (1024**3), 'GiB,')
            xmed = [min(df[xaxis]), max(df[xaxis])]
            if xlim: xmed = xlim
            plt.plot(xmed,
                     [ x * median for x in xmed ],
                     color=color,
                     alpha=.5)

        plt.plot(df[xaxis],
                 df[yaxis],
                 next(mark_style),
                 color=color,
                 label=labels[i])
    
    plt.xscale(xscale, base=xscale_base)
    plt.yscale(yscale, base=yscale_base)
    if not minor_ticks:
        plt.minorticks_off()
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    if xticks: plt.xticks(xticks)
    if yticks: plt.yticks(yticks)
    plt.grid(axis='y')
    plt.legend(labelspacing=legend_spacing)
    plt.title(title)
    if square: ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    if xlim: plt.xlim(xlim)
    if ylim: plt.ylim(ylim)
    if filename:
        plt.savefig(filename, bbox_inches='tight')
    if show:
        plt.show()
