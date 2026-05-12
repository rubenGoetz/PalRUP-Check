
##
## Generate plots for SAT'26 Paper "A Natively Parallel Proof Framework for Clause-Sharing SAT Solving"
## Data can be found at: TODO
##

import pandas as pd
import plot_lib as plots
import statistics as stats

SHOW=True

# read data
cadical_columns = ['name', 'result', 'runtime_solve', 'proof_bytes', 'runtime_check']
mono_columns = ['name', 'result', 'runtime_solve', 'runtime_assembly', 'proof_bytes', 'runtime_check']
palrup_columns = ['name', 'result', 'runtime_solve', 'runtime_check', 'runtime_val', 'success_palrup', 'runtime_lc', 'runtime_redist', 'runtime_con', 'waittime_lc', 'waittime_redist', 'waittime_con', 'proof_bytes', 'proxy_bytes', 'import_bytes', 'palrup_bytes_iqr', 'proxy_bytes_iqr', 'import_bytes_iqr']

dtype_map = {'proof_bytes': 'Int64'}

cadical_lrat = pd.read_csv('qresults-cadical-lrat.txt', sep=' ', names=cadical_columns, dtype=dtype_map, header=None)
cadical_lrat = cadical_lrat[cadical_lrat["result"] != 'UNKNOWN']
cadical_solve_1 = pd.read_csv('qresults-cadical-solve-1node.txt', sep=' ', names=['name', 'result', 'runtime_solve'], header=None)
cadical_solve_16 = pd.read_csv('qresults-cadical-solve-16node.txt', sep=' ', names=['name', 'result', 'runtime_solve'], header=None)
mono_1 = pd.read_csv('qresults-monolithic-1node.txt', sep=' ', names=mono_columns, dtype=dtype_map, header=None)
mono_16 = pd.read_csv('qresults-monolithic-16nodes.txt', sep=' ', names=mono_columns, dtype=dtype_map, header=None)
palrup_1 = pd.read_csv('qresults-proof-check-1node.txt', sep=' ', names=palrup_columns, dtype=dtype_map, header=None)
palrup_16 = pd.read_csv('qresults-proof-check-16node.txt', sep=' ', names=palrup_columns, dtype=dtype_map, header=None)
palrup_64 = pd.read_csv('qresults-proof-check-64node.txt', sep=' ', names=palrup_columns, dtype=dtype_map, header=None)

# define PAR-2 score
def par2(df, timeout=300, N=400):
    df = df[df['runtime_solve'].notnull()]
    
    nb_sat = len(df[df['result'] == 'SATISFIABLE'])
    nb_unsat = len(df[df['result'] == 'UNSATISFIABLE'])

    runtime_sat = sum(df[df['result'] == 'SATISFIABLE']['runtime_solve'])
    runtime_unsat = sum(df[df['result'] == 'UNSATISFIABLE']['runtime_solve'])

    res = {'ALL': (runtime_sat + runtime_unsat + ((N - nb_sat - nb_unsat) * timeout * 2)) / N,
           'SAT': (runtime_sat + (((N / 2) - nb_sat) * timeout * 2)) / (N / 2),
           'UNSAT': (runtime_unsat + (((N / 2) - nb_unsat) * timeout * 2)) / (N / 2)}

    return res

# Fig. 4
plots.plot_checker_components_runtime(dfs=[palrup_1, palrup_16, palrup_64],
                                      titles=['1 node', '16 nodes', '64 nodes'],
                                      show=SHOW,
                                      filename='runtime_checking_stages.pdf')


# Table 1
dfs = [cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
table = pd.DataFrame({'Solver': ['CaDiCaL', 'Monolithic 1 node', 'Monolithic 16 nodes', 'PalRUP 1 node', 'PalRUP 16 nodes', 'PalRUP 64 nodes']})
table['#'] = [ len(df[df['proof_bytes'].notnull()]) for df in dfs ]
table['p10'] = [ round(stats.quantiles(df[df['proof_bytes'].notnull()]['proof_bytes'], n=10)[0] / (1024**3), 3) for df in dfs ]
table['median'] = [ round(stats.median(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
table['mean'] = [ round(stats.mean(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
table['p90'] = [ round(stats.quantiles(df[df['proof_bytes'].notnull()]['proof_bytes'], n=10)[-1] / (1024**3), 1) for df in dfs ]
table['max'] = [ round(max(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
print('Table 1:')
print(table.astype(str).to_latex())


# Fig. 5 left
plots.plot_CDF(dfs=[palrup_64, cadical_solve_16, palrup_16, mono_16, cadical_solve_1, palrup_1, mono_1, cadical_lrat[cadical_lrat['runtime_solve'] <= 300]],
               labels=['PalRUP-64', 'MallobSat-16', 'PalRUP-16', 'Mono-16', 'MallobSat-1', 'PalRUP-1', 'Mono-1', "CaDiCaL"],
               line_styles=['-', '--', ':', '-', '--', ':', '-.'],
               colors=['black', 'tab:red', 'tab:orange', 'tab:orange', 'darkblue', 'tab:blue', 'tab:blue', 'black'],
               xlim=300,
               ylim=325,
               figsize=[2.8, 3],
               square=False,
               show=SHOW,
               filename='fullsolvingcdf.pdf')


# Fig.5 right (Table)
dfs = [cadical_solve_1, cadical_solve_16, cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
table = pd.DataFrame({'Setup': ['N 1x48', 'N 16x48', 'SP 1', 'SP 1x48', 'SP 16x48', 'PP 1x48', 'PP 16x48', 'PP 64x48'],
                      'Solver': ['MallobSat', 'MallobSat', 'CaDiCaL', 'Monolithic', 'Monolithic', 'PalRUP', 'PalRUP', 'PalRUP']})
table['ALL #'] = [ len(df[df['result'] != 'UNKNOWN']) for df in dfs ]
table['ALL PAR'] = [ round(par2(df)['ALL'], 1) for df in dfs ]
table['SAT #'] = [ len(df[df['result'] == 'SATISFIABLE']) for df in dfs ]
table['SAT PAR'] = [ round(par2(df)['SAT'], 1) for df in dfs ]
table['UNSAT #'] = [ len(df[df['result'] == 'UNSATISFIABLE']) for df in dfs ]
table['UNSAT PAR'] = [ round(par2(df)['UNSAT'], 1) for df in dfs ]
print('Table in Fig. 5:')
print(table.astype(str).to_latex())


# Fig. 6 left
plots.plot_tight_square(dfs=[mono_1, mono_16, palrup_1, palrup_16, palrup_64, cadical_lrat[cadical_lrat['runtime_solve'] <= 300]],
                        labels=['Mono-1', 'Mono-16', 'PalRUP-1', 'PalRUP-16', 'PalRUP-64', 'CaDiCaL'],
                        marks=['.', '+', '.', '+', 'x', 'x'],
                        colors=['tab:blue', 'tab:cyan', 'tab:green', 'tab:orange', 'black', 'tab:red'],
                        title='',
                        show=SHOW,
                        filename='square_plot.pdf')


# Fig. 6 right
cadical_lrat['runtime_end2end'] = [ s + c for s, c in zip(cadical_lrat['runtime_solve'], cadical_lrat['runtime_check']) ]
mono_1['runtime_end2end'] = [ s + a + c for s, a, c in zip(mono_1['runtime_solve'], mono_1['runtime_assembly'], mono_1['runtime_check']) ]
mono_16['runtime_end2end'] = [ s + a + c for s, a, c in zip(mono_16['runtime_solve'], mono_16['runtime_assembly'], mono_16['runtime_check']) ]
palrup_1['runtime_end2end'] = [ s + c for s, c in zip(palrup_1['runtime_solve'], palrup_1['runtime_check']) ]
palrup_16['runtime_end2end'] = [ s + c for s, c in zip(palrup_16['runtime_solve'], palrup_16['runtime_check']) ]
palrup_64['runtime_end2end'] = [ s + c for s, c in zip(palrup_64['runtime_solve'], palrup_64['runtime_check']) ]
plots.plot_CDF(dfs=[palrup_64[palrup_64['success_palrup'] == True],
                    palrup_16[palrup_16['success_palrup'] == True],
                    palrup_1[palrup_1['success_palrup'] == True],
                    mono_16, mono_1, cadical_lrat],
               labels=['PalRUP-64', 'PalRUP-16', 'PalRUP-1', 'Mono-16', 'Mono-1', 'CaDiCaL'],
               line_styles=[':', '-', '--', '-', '--', '-'],
               colors=['tab:blue']*3 + ['tab:orange']*2 + ['black'],
               column_to_plot='runtime_end2end',
               xlim=360,
               ylim=140,
               legend_spacing=0,
               show=SHOW,
               filename='end2end_runtimes.pdf')

# Appendix Figures

# Fig. 7
plots.plot_boxplots([palrup_1, palrup_16, palrup_64],
                    ['palrup_bytes_iqr', 'proxy_bytes_iqr', 'import_bytes_iqr'],
                    ['Proof', 'Import', 'Redist'],
                    titles=['1 node (48 cores)', '16 nodes (768 cores)', '64 nodes (3072 cores)'],
                    filename='proof_dist.pdf')


# Fig. 8
plots.plot_scatter([palrup_64, palrup_16, palrup_1, cadical_lrat],
                   ['PalRUP-64', 'PalRUP-16', 'PalRUP-1', 'CaDiCaL'],
                   xaxis='runtime_solve',
                   yaxis='proof_bytes',
                   xlim=[1, 300],
                   ylim=[10**6, 10**13],
                   xscale_base=2,
                   xticks=[ 2**i for i in range(0,9) ],
                   square=False,
                   figsize=[2.75, 2.75],
                   plot_median=True,
                   filename='throughput.pdf')      
