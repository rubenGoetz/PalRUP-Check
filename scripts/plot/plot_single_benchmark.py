
import sys

import pandas as pd
import statistics as stat

from matplotlib import pyplot as plt

benchmark_logs_path = sys.argv[1]

df_runtimes = pd.read_csv(f"{benchmark_logs_path}/runtimes.csv")
instances = sorted(list(set(df_runtimes['instance'])))

for i, instance in enumerate(instances):
    df = df_runtimes[df_runtimes['instance'] == instance]
    plt.boxplot(df['runtime'], label=instance, positions=[i/len(instances)])
plt.title('runtime distributions')
plt.legend(loc='center left', bbox_to_anchor=(1, 0.5), handlelength=.5)
plt.savefig(f"{benchmark_logs_path}/boxplot.pdf", bbox_inches='tight')

for i, instance in enumerate(instances):
    df = df_runtimes[df_runtimes['instance'] == instance]
    print(instance)
    print('    min   :', min(df['runtime']))
    print('    mean  :', stat.mean(df['runtime']))
    print('    median:', stat.median(df['runtime']))
    print('    max   :', max(df['runtime']))

print()
print('boxplot:', f"{benchmark_logs_path}/boxplot.pdf")
