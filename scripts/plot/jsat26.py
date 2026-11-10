
##
## Generate additional plots for JSAT'26 journal version Paper
## "A Natively Parallel Proof Framework for Clause-Sharing SAT Solving"
## Data available at: TODO
##

import pandas as pd
import plot_lib as plots
import statistics as stats
import collections

SHOW=True

# read data
label_map = {'d': 'drup',
             'c': 'conv',
             'l': 'lrup',
             'D': 'drup-conv',
             'b': 'best',
             'B': 'best-conv'}
symbol_map = dict([ (label_map[key], key) for key in label_map])

#dtype_map = {'proof_bytes': 'Int64'}
# k: only kissat
# p: portfolio
# TODO
# dfs[scale][solver][checking]
dfs = collections.defaultdict(lambda : collections.defaultdict(dict))
dfs[1]['k']['s'] = pd.read_csv('qresults-1ks.csv')  # contains pure solving data. Redundant but useful.
dfs[1]['k']['d'] = pd.read_csv('qresults-1kd.csv')
dfs[1]['k']['c'] = pd.read_csv('qresults-1kc.csv')
dfs[1]['k']['l'] = pd.read_csv('qresults-1kl.csv')
dfs[1]['k']['D'] = pd.read_csv('qresults-1kD.csv')

dfs[16]['k']['s'] = pd.read_csv('qresults-16ks.csv')
dfs[16]['k']['d'] = pd.read_csv('qresults-16kd.csv')
dfs[16]['k']['c'] = pd.read_csv('qresults-16kc.csv')
dfs[16]['k']['l'] = pd.read_csv('qresults-16kl.csv')
dfs[16]['k']['D'] = pd.read_csv('qresults-16kD.csv')

#dfs[1]['p']['s'] = pd.read_csv('qresults-1ks.csv')
#dfs[1]['p']['d'] = pd.read_csv('qresults-1kd.csv')
#dfs[1]['p']['c'] = pd.read_csv('qresults-1kc.csv')
#dfs[1]['p']['l'] = pd.read_csv('qresults-1kl.csv')
#dfs[1]['p']['D'] = pd.read_csv('qresults-1kD.csv')

#dfs[16]['p']['s'] = pd.read_csv('qresults-1ks.csv')
#dfs[16]['p']['d'] = pd.read_csv('qresults-1kd.csv')
#dfs[16]['p']['c'] = pd.read_csv('qresults-1kc.csv')
#dfs[16]['p']['l'] = pd.read_csv('qresults-1kl.csv')
#dfs[16]['p']['D'] = pd.read_csv('qresults-1kD.csv')

dfs[1]['p']['s'] = pd.DataFrame({'runtime_solve': [0]})
dfs[1]['p']['d'] = pd.DataFrame({'runtime_solve': [0]})
dfs[1]['p']['c'] = pd.DataFrame({'runtime_solve': [0]})
dfs[1]['p']['l'] = pd.DataFrame({'runtime_solve': [0]})
dfs[1]['p']['D'] = pd.DataFrame({'runtime_solve': [0]})

dfs[16]['p']['s'] = pd.DataFrame({'runtime_solve': [0]})
dfs[16]['p']['d'] = pd.DataFrame({'runtime_solve': [0]})
dfs[16]['p']['c'] = pd.DataFrame({'runtime_solve': [0]})
dfs[16]['p']['l'] = pd.DataFrame({'runtime_solve': [0]})
dfs[16]['p']['D'] = pd.DataFrame({'runtime_solve': [0]})


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


# Table 1
#dfs = [cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
#table = pd.DataFrame({'Solver': ['CaDiCaL', 'Monolithic 1 node', 'Monolithic 16 nodes', 'PalRUP 1 node', 'PalRUP 16 nodes', 'PalRUP 64 nodes']})
#table['#'] = [ len(df[df['proof_bytes'].notnull()]) for df in dfs ]
#table['p10'] = [ round(stats.quantiles(df[df['proof_bytes'].notnull()]['proof_bytes'], n=10)[0] / (1024**3), 3) for df in dfs ]
#table['median'] = [ round(stats.median(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
#table['mean'] = [ round(stats.mean(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
#table['p90'] = [ round(stats.quantiles(df[df['proof_bytes'].notnull()]['proof_bytes'], n=10)[-1] / (1024**3), 1) for df in dfs ]
#table['max'] = [ round(max(df[df['proof_bytes'].notnull()]['proof_bytes']) / (1024**3), 1) for df in dfs ]
#print('Table 1:')
#print(table.astype(str).to_latex())

plots.plot_CDF(dfs=[dfs[16]['p']['s'], dfs[16]['k']['s'], dfs[1]['p']['s'], dfs[1]['k']['s']],
               labels=['P-PalRUP-16', 'K-PalRUP-16', 'P-PalRUP-1', 'K-PalRUP-1'],
               #line_styles=['-', '--', ':', '-', '--', ':', '-.'],
               line_styles=['--', '-', '--', '-'],
               #colors=['black', 'tab:red', 'tab:orange', 'tab:orange', 'darkblue', 'tab:blue', 'tab:blue', 'black'],
               colors=['tab:red', 'tab:orange', 'darkblue', 'tab:blue'],
               xlim=300,
               ylim=325,
               figsize=[2.8, 3],
               square=False,
               show=SHOW,
               filename='fullsolvingcdf.pdf')


# Fig. 4 right (Table)
#dfs = [cadical_solve_1, cadical_solve_16, cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
#table = pd.DataFrame({'Setup': ['N 1x48', 'N 16x48', 'SP 1', 'SP 1x48', 'SP 16x48', 'PP 1x48', 'PP 16x48', 'PP 64x48'],
#                      'Solver': ['MallobSat', 'MallobSat', 'CaDiCaL', 'Monolithic', 'Monolithic', 'PalRUP', 'PalRUP', 'PalRUP']})
#table['ALL #'] = [ len(df[df['result'] != 'UNKNOWN']) for df in dfs ]
#table['ALL PAR'] = [ round(par2(df)['ALL'], 1) for df in dfs ]
#table['SAT #'] = [ len(df[df['result'] == 'SATISFIABLE']) for df in dfs ]
#table['SAT PAR'] = [ round(par2(df)['SAT'], 1) for df in dfs ]
#table['UNSAT #'] = [ len(df[df['result'] == 'UNSATISFIABLE']) for df in dfs ]
#table['UNSAT PAR'] = [ round(par2(df)['UNSAT'], 1) for df in dfs ]
#print('Table in Fig. 5:')
#print(table.astype(str).to_latex())

dfs_filtered  = [ df[df['success_palrup'] ==  True] for df in [dfs[16]['k']['d']] ]
dfs_filtered += [ df[df['success_palrup'] != False] for df in [dfs[16]['k']['c']] ]
dfs_filtered += [ df[df['success_palrup'] ==  True] for df in [dfs[16]['k']['l']] ]
dfs_filtered += [ df[df['success_palrup'] ==  True] for df in [dfs[16]['k']['D']] ]
plots.plot_tight_square(dfs=dfs_filtered,
                        labels=['Kissat-d-16', 'Kissat-c-16', 'Kissat-l-16', 'Kissat-D-16'],
                        marks=['.', '+', '.', '+', 'x', 'x'],
                        colors=['tab:blue', 'tab:cyan', 'tab:green', 'tab:orange', 'black', 'tab:red'],
                        title='',
                        show=SHOW,
                        filename='square_plot.pdf')

dfs_filtered  = [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['d']] ]
dfs_filtered += [ df[df['success_palrup'] != False] for df in [dfs[1]['k']['c']] ]
dfs_filtered += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['l']] ]
dfs_filtered += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['D']] ]
plots.plot_tight_square(dfs=dfs_filtered,
                        labels=['Kissat-d-1', 'Kissat-c-1', 'Kissat-l-1', 'Kissat-D-1'],
                        marks=['.', '+', '.', '+', 'x', 'x'],
                        colors=['tab:blue', 'tab:cyan', 'tab:green', 'tab:orange', 'black', 'tab:red'],
                        title='',
                        show=SHOW,
                        filename='square_plot.pdf')


# Fig. 5 right
#cadical_lrat['runtime_end2end'] = [ s + c for s, c in zip(cadical_lrat['runtime_solve'], cadical_lrat['runtime_check']) ]
#mono_1['runtime_end2end'] = [ s + a + c for s, a, c in zip(mono_1['runtime_solve'], mono_1['runtime_assembly'], mono_1['runtime_check']) ]
#mono_16['runtime_end2end'] = [ s + a + c for s, a, c in zip(mono_16['runtime_solve'], mono_16['runtime_assembly'], mono_16['runtime_check']) ]
#palrup_1['runtime_end2end'] = [ s + c for s, c in zip(palrup_1['runtime_solve'], palrup_1['runtime_check']) ]
#palrup_16['runtime_end2end'] = [ s + c for s, c in zip(palrup_16['runtime_solve'], palrup_16['runtime_check']) ]
#palrup_64['runtime_end2end'] = [ s + c for s, c in zip(palrup_64['runtime_solve'], palrup_64['runtime_check']) ]
#plots.plot_CDF(dfs=[palrup_64[palrup_64['success_palrup'] == True],
#                    palrup_16[palrup_16['success_palrup'] == True],
#                    palrup_1[palrup_1['success_palrup'] == True],
#                    mono_16, mono_1, cadical_lrat],
#               labels=['PalRUP-64', 'PalRUP-16', 'PalRUP-1', 'Mono-16', 'Mono-1', 'CaDiCaL'],
#               line_styles=[':', '-', '--', '-', '--', '-'],
#               colors=['tab:blue']*3 + ['tab:orange']*2 + ['black'],
#               column_to_plot='runtime_end2end',
#               xlim=360,
#               ylim=140,
#               legend_spacing=0,
#               show=SHOW,
#               filename='end2end_runtimes.pdf')

# Fig. 6
#plots.plot_checker_components_runtime(dfs=[palrup_1, palrup_16, palrup_64],
#                                      titles=['1 node', '16 nodes', '64 nodes'],
#                                      show=SHOW,
#                                      filename='runtime_checking_stages.pdf')


# Appendix Figures

