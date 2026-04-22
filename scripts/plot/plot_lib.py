
import itertools
import math

from matplotlib import rc
from matplotlib import pyplot as plt

###################
## Generic Plots ##
###################

def plot_square(df,
                xaxis='runtime_solve',
                yaxis='runtime_check',
                xlabel='solving time in s',
                ylabel='checking time in s',
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
    plt.show()

#####################
## SAT_2026_PALRUP ##
#####################

def plot_solved_over_time(dfs, labels,
                          column_to_plot='runtime_solve',
                          figsize=[5.5, 2.75],
                          line_styles=['-', '--'],
                          colors=['tab:blue', 'tab:blue', 'tab:orange', 'tab:orange'],
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
    print('xmax', xmax)
    print('ymax', ymax)
    
    fig = plt.figure(figsize=figsize)
    ax = fig.add_subplot()

    for i in range(0, len(dfs)):
        df = dfs[i].sort_values(column_to_plot)
        df = df[df[column_to_plot].notnull()]
        label = labels[i]

        plt.plot(list(df[column_to_plot]) + [xmax],
                 list(range(0, len(df))) + [len(df) - 1],
                 next(line_style),
                 label=label,
                 color=next(color))
        
    
    ax.axis([0, xmax, 0, ymax])
    ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    
    plt.xlabel('Runtime $t$ in s')
    plt.ylabel('Solved instances in $\leq t$')
    plt.xticks(range(0, math.ceil(xmax), 60))
    plt.grid()
    plt.legend()
    plt.tight_layout()
    if filename:
        plt.savefig(filename)
    plt.show()

def plot_tight_square(dfs,
                      labels='',
                xaxis='runtime_solve',
                xlabel='solving time in s',
                ylabel='checking time in s',
                marks=['+', '.', 'x', '*'],
                colors=['blue', 'orange', 'green', 'red'],
                title='X Nodes',
                figsize=[5.5, 2.75],
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
    
    print("_min:", _min, "_max:", _max)
    #ax.axis([_min, _max] * 2)
    ax.axis([_min, 300] * 2)
    plt.xscale('log')
    plt.yscale('log')
    ax.set_aspect(1.0/ax.get_data_ratio(), adjustable='box')
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend(loc='center left', bbox_to_anchor=(1, 0.5))
    plt.grid()
    if filename:
        plt.savefig(filename, bbox_inches='tight')
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
                label='overall runtime',
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


    fig.legend(labels=['runtime check']+labels, loc='center left', bbox_to_anchor=(1, 0.5), handlelength=.5)
    if len(dfs) > 1: ax = axs[i]
    else: ax = axs
    ax.set(xlabel='Instances sorted by accumulated runtime')
    fig.tight_layout()
    if filename:
       fig.savefig(filename, bbox_inches='tight')
    fig.show()
