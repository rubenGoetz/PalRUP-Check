
import glob
import sys

import pandas as pd
import statistics as stat

from matplotlib import pyplot as plt

path = sys.argv[1]
runs = glob.glob(f"{path}/run.*")
runs.sort(key=lambda s: int(s.split('.')[-1]))

dfs = []
for run in runs:
    dfs.append(pd.read_csv(f"{run}/runtimes.csv"))

instances = sorted([ path.split('/')[-1] for path in list(glob.glob('formulas/debug/*')) ])

min_runtimes = {}
mean_runtimes = {}
max_runtimes = {}

for instance in instances:
    min_runtimes[instance] = []
    mean_runtimes[instance] = []
    max_runtimes[instance] = []
    for df in dfs:
        if (len(df[df['instance'] == instance]) == 0):
            min_runtimes[instance].append(float('nan'))
            mean_runtimes[instance].append(float('nan'))
            max_runtimes[instance].append(float('nan'))
            continue
        min_runtimes[instance].append(min(df[df['instance'] == instance]['runtime']))
        mean_runtimes[instance].append(stat.mean(df[df['instance'] == instance]['runtime']))
        max_runtimes[instance].append(max(df[df['instance'] == instance]['runtime']))

fig, axs = plt.subplots(1, len(instances), figsize=[12, 5])

for i, instance in enumerate(instances):
    if len(instances) > 1: ax = axs[i]
    else: ax = axs

    ax.plot(range(len(max_runtimes[instance])), max_runtimes[instance], '.',
            label='max')
    ax.plot(range(len(mean_runtimes[instance])), mean_runtimes[instance], 'x',
            label='mean')
    ax.plot(range(len(min_runtimes[instance])), min_runtimes[instance], '*',
            label='min')

    ax.set(title=instance[:5])
    ax.set_box_aspect(1)

axs[-1].legend(loc='center left', bbox_to_anchor=(1, 0.5), handlelength=.5)
#fig.legend(['min', 'mean', 'max'])

# set single y label
axs[0].set(ylabel='runtime in s')

# set single x axis in the middle
if len(dfs) > 1: ax = axs[int(len(instances) / 2)]
else: ax = axs.set(xlabel='run number')
ax.set(xlabel='run number')
fig.tight_layout()

plt.savefig('benchmark_logs/progression.pdf', bbox_inches='tight')

print('progression plot:', 'benchmark_logs/progression.pdf')
