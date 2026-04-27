
##
## Generate plots for SAT'26 Paper "A Natively Parallel Proof Framework for Clause-Sharing SAT Solving"
## Data can be found at: TODO
##

import pandas as pd
import plot_lib as plots

SHOW=True

# read data
cadical_columns = ['name', 'result', 'runtime_solve', 'proof_bytes', 'runtime_check']
mono_columns = ['name', 'result', 'runtime_solve', 'runtime_assembly', 'proof_bytes', 'runtime_check']
palrup_columns = ['name', 'result', 'runtime_solve', 'runtime_check', 'runtime_val', 'success_palrup', 'runtime_lc', 'runtime_redist', 'runtime_con', 'waittime_lc', 'waittime_redist', 'waittime_con', 'palrup_bytes', 'proxy_bytes', 'import_bytes']

cadical_lrat = pd.read_csv('qresults-cadical-lrat.txt', sep=' ', names=cadical_columns, header=None)
cadical_lrat = cadical_lrat[cadical_lrat["result"] != 'UNKNOWN']
cadical_solve_1 = pd.read_csv('qresults-cadical-solve-1node.txt', sep=' ', names=['name', 'result', 'runtime_solve'], header=None)
cadical_solve_16 = pd.read_csv('qresults-cadical-solve-16node.txt', sep=' ', names=['name', 'result', 'runtime_solve'], header=None)
mono_1 = pd.read_csv('qresults-monolithic-1node.txt', sep=' ', names=mono_columns, header=None)
mono_16 = pd.read_csv('qresults-monolithic-16nodes.txt', sep=' ', names=mono_columns, header=None)
palrup_1 = pd.read_csv('qresults-proof-check-1node.txt', sep=' ', names=palrup_columns, header=None)
palrup_16 = pd.read_csv('qresults-proof-check-16node.txt', sep=' ', names=palrup_columns, header=None)
palrup_64 = pd.read_csv('qresults-proof-check-64node.txt', sep=' ', names=palrup_columns, header=None)

# define PAR-2 score
def par2(df, timeout=300, N=400):
    df = df[df['runtime_solve'].notnull()]
    
    nb_sat = len(df[df['result'] == 'SATISFIABLE'])
    nb_unsat = len(df[df['result'] == 'UNSATISFIABLE'])

    #runtime_sat = sum(df[(df['result'] == 'SATISFIABLE') & (df['runtime_solve'].notnull())]['runtime_solve'])
    #runtime_unsat = sum(df[(df['result'] == 'UNSATISFIABLE') & (df['runtime_solve'].notnull())]['runtime_solve'])

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


# Fig. 5 left
plots.plot_CDF(dfs=[cadical_solve_16, palrup_16, mono_16, cadical_solve_1, palrup_1, mono_1, cadical_lrat[cadical_lrat['runtime_solve'] <= 300]],
               labels=['MallobSat-16', 'PalRUP-16', 'Mono-16', 'MallobSat-1', 'PalRUP-1', 'Mono-1', "CaDiCaL"],
               line_styles=['-', '--', ':', '-', '--', ':', '-.'],
               colors=['tab:red', 'tab:orange', 'tab:orange', 'darkblue', 'tab:blue', 'tab:blue', 'black'],
               xlim=300,
               show=SHOW,
               filename='fullsolvingcdf.pdf')


# Fig.5 right (Table)
table = pd.DataFrame({'Setup': ['N_1x48', 'N_16x48', 'SP_1', 'SP_1x48', 'SP_16x48', 'PP_1x48', 'PP_16x48', 'PP_64x48'],
                      'Solver': ['MallobSat', 'MallobSat', 'CaDiCaL', 'Monolithic', 'Monolithic', 'PalRUP', 'PalRUP', 'PalRUP']})
dfs = [cadical_solve_1, cadical_solve_16, cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
table['ALL_#'] = [ len(df[df['result'] != 'UNKNOWN']) for df in dfs ]
table['ALL_PAR'] = [ par2(df)['ALL'] for df in dfs ]
table['SAT_#'] = [ len(df[df['result'] == 'SATISFIABLE']) for df in dfs ]
table['SAT_PAR'] = [ par2(df)['SAT'] for df in dfs ]
table['UNSAT_#'] = [ len(df[df['result'] == 'UNSATISFIABLE']) for df in dfs ]
table['UNSAT_PAR'] = [ par2(df)['UNSAT'] for df in dfs ]
print(table.to_latex())


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
               ylim=150,
               show=SHOW,
               filename='end2end_runtimes.pdf')
