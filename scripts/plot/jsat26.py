
##
## Generate additional plots for JSAT'26 journal version Paper
## "A Natively Parallel Proof Framework for Clause-Sharing SAT Solving"
## Data available at: TODO
##

import pandas as pd
import plot_lib as plots
import statistics as stats
import collections

SHOW=False

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
# TODO: ref figures/tables correctly
# dfs[scale][solver][checking]
dfs = collections.defaultdict(lambda : collections.defaultdict(dict))
dfs[1]['k']['s'] = pd.read_csv('qresults-1ks.csv')  # contains pure solving data. Redundant but useful.
dfs[1]['k']['d'] = pd.read_csv('qresults-1kd.csv')
dfs[1]['k']['c'] = pd.read_csv('qresults-1kc.csv')
dfs[1]['k']['l'] = pd.read_csv('qresults-1kl.csv')
dfs[1]['k']['D'] = pd.read_csv('qresults-1kD.csv')

#dfs[16]['k']['s'] = pd.read_csv('qresults-16ks.csv')
#dfs[16]['k']['d'] = pd.read_csv('qresults-16kd.csv')
#dfs[16]['k']['c'] = pd.read_csv('qresults-16kc.csv')
#dfs[16]['k']['l'] = pd.read_csv('qresults-16kl.csv')
#dfs[16]['k']['D'] = pd.read_csv('qresults-16kD.csv')

dfs[16]['k']['s'] = pd.DataFrame({'runtime_solve': [0], 'runtime_check': [0]})
dfs[16]['k']['d'] = pd.DataFrame({'runtime_solve': [0], 'runtime_check': [0]})
dfs[16]['k']['c'] = pd.DataFrame({'runtime_solve': [0], 'runtime_check': [0]})
dfs[16]['k']['l'] = pd.DataFrame({'runtime_solve': [0], 'runtime_check': [0]})
dfs[16]['k']['D'] = pd.DataFrame({'runtime_solve': [0], 'runtime_check': [0]})

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
table_dfs = [dfs[1]['k']['d'], dfs[1]['k']['l']]
table = pd.DataFrame({'Solver': ['k-PalRUP 1 node (blind)', 'k-PalRUP 1 node (elaborated)']})
table['#'] = [ len(df[df['palrup_bytes'].notnull()]) for df in table_dfs ]
table['p10'] = [ round(stats.quantiles(df[df['palrup_bytes'].notnull()]['palrup_bytes'], n=10)[0] / (1024**3), 3) for df in table_dfs ]
table['median'] = [ round(stats.median(df[df['palrup_bytes'].notnull()]['palrup_bytes']) / (1024**3), 1) for df in table_dfs ]
table['mean'] = [ round(stats.mean(df[df['palrup_bytes'].notnull()]['palrup_bytes']) / (1024**3), 1) for df in table_dfs ]
table['p90'] = [ round(stats.quantiles(df[df['palrup_bytes'].notnull()]['palrup_bytes'], n=10)[-1] / (1024**3), 1) for df in table_dfs ]
table['max'] = [ round(max(df[df['palrup_bytes'].notnull()]['palrup_bytes']) / (1024**3), 1) for df in table_dfs ]
print('Table 1:')
print(table.astype(str).to_latex())

plots.plot_CDF(dfs=[dfs[16]['p']['s'], dfs[16]['k']['s'], dfs[1]['p']['s'], dfs[1]['k']['s']],
               labels=['p-PalRUP-16', 'k-PalRUP-16', 'p-PalRUP-1', 'k-PalRUP-1'],
               #line_styles=['-', '--', ':', '-', '--', ':', '-.'],
               line_styles=['--', '-', '--', '-'],
               #colors=['black', 'tab:red', 'tab:orange', 'tab:orange', 'darkblue', 'tab:blue', 'tab:blue', 'black'],
               colors=['tab:red', 'tab:orange', 'darkblue', 'tab:blue'],
               xlim=300,
               ylim=325,
               figsize=[2.8, 3],
               square=False,
               show=SHOW,
               filename='fullsolvingcdf-drup.pdf')

# Fig. 4 right (Table)
#dfs = [cadical_solve_1, cadical_solve_16, cadical_lrat, mono_1, mono_16, palrup_1, palrup_16, palrup_64]
table_dfs = [dfs[1]['k']['s']]
table = pd.DataFrame({'Setup': ['PP 1x48'],
                      'Solver': ['k-PalRUP']})
table['ALL #'] = [ len(df[df['result'].notnull()]) for df in table_dfs ]
table['ALL PAR'] = [ round(par2(df)['ALL'], 1) for df in table_dfs ]
table['SAT #'] = [ len(df[df['result'] == 'SATISFIABLE']) for df in table_dfs ]
table['SAT PAR'] = [ round(par2(df)['SAT'], 1) for df in table_dfs ]
table['UNSAT #'] = [ len(df[df['result'] == 'UNSATISFIABLE']) for df in table_dfs ]
table['UNSAT PAR'] = [ round(par2(df)['UNSAT'], 1) for df in table_dfs ]
print('Table in Fig. 5:')
print(table.astype(str).to_latex())


dfs_filtered_1node  = [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['d']] ]
dfs_filtered_1node += [ df[df['success_palrup'] != False] for df in [dfs[1]['k']['c']] ]
dfs_filtered_1node += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['D']] ]
dfs_filtered_1node += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['l']] ]
plots.plot_tight_square(dfs=dfs_filtered_1node,
                        labels=['PalRUP-d', 'PalRUP-c', 'PalRUP-D', 'PalRUP-l'],
                        marks=['.', '*', '+', '.'],
                        colors=['tab:blue', 'tab:cyan', 'tab:orange', 'black', 'tab:red'],
                        title='',
                        legend_outside=False,
                        show=SHOW,
                        filename='square_plot_1node_kissat.pdf')

# dfs_filtered_16node  = [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['d']] ]
# dfs_filtered_16node += [ df[df['success_palrup'] != False] for df in [dfs[1]['k']['c']] ]
# dfs_filtered_16node += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['D']] ]
# dfs_filtered_16node += [ df[df['success_palrup'] ==  True] for df in [dfs[1]['k']['l']] ]
# plots.plot_tight_square(dfs=dfs_filtered_16node,
#                         labels=['Kissat-d-16', 'Kissat-c-16', 'Kissat-D-16', 'Kissat-l-16'],
#                         marks=['.', '*', '+', '.'],
#                         colors=['tab:blue', 'tab:cyan', 'tab:orange', 'black', 'tab:red'],
#                         title='',
#                         show=SHOW,
#                         filename='square_plot_16node_kissat.pdf')

#dfs_filtered_1node[0].to_csv('jsat_filtered.csv')

table = pd.DataFrame({'Mode': ['d', 'c', 'D', 'l']})
table['1x48 k-PalRUP'] = [ round(stats.geometric_mean([ x/y for x,y in zip(df['runtime_check'], df['runtime_solve']) ]), 2) for df in dfs_filtered_1node ]
print('Table gm overhead:')
print(table.transpose().astype(str).to_latex())

