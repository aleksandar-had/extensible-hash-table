import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# data files of the case where all threads have their own local set of elements to operate on
filenames_local = ["local/TimeData_1_threads_1000_mseconds_12062022151633.csv",
                   "local/TimeData_2_threads_1000_mseconds_12062022151744.csv",
                   "local/TimeData_4_threads_1000_mseconds_12062022151920.csv",
                   "local/TimeData_6_threads_1000_mseconds_12062022152033.csv",
                   "local/TimeData_8_threads_1000_mseconds_12062022152149.csv",
                   "local/TimeData_12_threads_1000_mseconds_12062022152307.csv",
                   "local/TimeData_16_threads_1000_mseconds_12062022152423.csv",
                   "local/TimeData_24_threads_1000_mseconds_12062022152537.csv",
                   "local/TimeData_32_threads_1000_mseconds_12062022153053.csv",
                   "local/TimeData_40_threads_1000_mseconds_12062022153310.csv",
                   "local/TimeData_48_threads_1000_mseconds_12062022153434.csv",
                   "local/TimeData_56_threads_1000_mseconds_12062022153549.csv",
                   "local/TimeData_64_threads_1000_mseconds_12062022153749.csv"]

# data file of the case where all threads operate on the same set of elements i.e. global
filenames_global = ["global/TimeData_1_threads_1000_mseconds_12062022151350.csv",
                    "global/TimeData_2_threads_1000_mseconds_12062022151311.csv",
                    "global/TimeData_4_threads_1000_mseconds_12062022151234.csv",
                    "global/TimeData_6_threads_1000_mseconds_12062022151152.csv",
                    "global/TimeData_8_threads_1000_mseconds_12062022151108.csv",
                    "global/TimeData_12_threads_1000_mseconds_12062022151028.csv",
                    "global/TimeData_16_threads_1000_mseconds_12062022150949.csv",
                    "global/TimeData_24_threads_1000_mseconds_12062022150910.csv",
                    "global/TimeData_32_threads_1000_mseconds_12062022150828.csv",
                    "global/TimeData_40_threads_1000_mseconds_12062022150742.csv",
                    "global/TimeData_48_threads_1000_mseconds_12062022150656.csv",
                    "global/TimeData_56_threads_1000_mseconds_12062022150556.csv",
                    "global/TimeData_64_threads_1000_mseconds_12062022150409.csv"]

filenames = filenames_global
num_iterations = 30
lock_free_througput = [
    [0 for j in range(len(filenames))] for i in range(num_iterations)]
lock_based_througput = [
    [0 for j in range(len(filenames))] for i in range(num_iterations)]
for i in range(len(filenames)):
    f = open(filenames[i], "r")
    second_line = f.read().split("\n")[1]
    times = second_line.split(",")
    num_threads = float(times[0])
    # made in error in thread global measurement and only took half the time
    # time = float(times[1])/2
    time = float(times[1])
    for j in range(num_iterations):
        lock_free_ops = float(times[2+j*2])
        lock_based_ops = float(times[2+j*2+1])
        lock_free_througput[j][i] = (
            lock_free_ops / time) * 10**-3  # ops/msec
        lock_based_througput[j][i] = (
            lock_based_ops / time) * 10**-3  # ops/msec

df_lock_based = pd.DataFrame(data=lock_based_througput, columns=[
    "1", "2", "4", "6", "8", "12", "16", "24", "32", "40", "48", "56", "64"])

df_lock_free = pd.DataFrame(data=lock_free_througput, columns=[
    "1", "2", "4", "6", "8", "12", "16", "24", "32", "40", "48", "56", "64"])

fig, (ax1, ax2) = plt.subplots(1, 2)

sns.barplot(ax=ax1, x="variable", y="value",
            data=pd.melt(df_lock_free), capsize=.2, errwidth=1)
ax1.set(xlabel="threads", ylabel='ops/msec')

sns.barplot(ax=ax2, x="variable", y="value",
            data=pd.melt(df_lock_based), capsize=.2, errwidth=1)
ax2.set(xlabel="threads", ylabel='ops/msec')

upper_ylimit = 50000
ax1.set_ylim([0, upper_ylimit])
ax2.set_ylim([0, upper_ylimit])

ax1.set_title('Lock-free Hash Table')
ax2.set_title('Lock-based Hash Table')

fig.set_size_inches(12, 6)
plt.show()

### Var Load Factor ###
filenames_var = ["var2/TimeData_64_threads_1000_mseconds_var_load_factor1.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor2.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor3.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor4.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor5.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor6.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor7.csv",
                    "var2/TimeData_64_threads_1000_mseconds_var_load_factor8.csv"]
var_iterations = 38
lock_free_througput_var = [
    [0 for j in range(len(filenames_var))] for i in range(var_iterations)]
lock_based_througput_var = [
    [0 for j in range(len(filenames_var))] for i in range(var_iterations)]
for i in range(len(filenames_var)):
    f = open(filenames_var[i], "r")
    second_line_var = f.read().split("\n")[1]
    times_var = second_line_var.split(",")
    num_threads = 64
    # made in error in thread global measurement and only took half the time
    # time = float(times[1])/2
    time = float(times_var[1])
    for j in range(num_iterations):
        lock_free_ops = float(times_var[2+j*2])
        lock_based_ops = float(times_var[2+j*2+1])
        lock_free_througput_var[j][i] = (
            lock_free_ops / time) * 10**-3  # ops/msec
        lock_based_througput_var[j][i] = (
            lock_based_ops / time) * 10**-3  # ops/msec

df_lock_based_var = pd.DataFrame(data=lock_based_througput_var, columns=[
    "0/0/1", "0.2/0/0.8", "0.4/0/0.6", "0.6/0/0.4", "0.8/0/0.2","0.9/0.1/0","0.7/0.3/0", "0.2/0.05/0.75"])
df_lock_free_var = pd.DataFrame(data=lock_free_througput_var, columns=[
    "0/0/1", "0.2/0/0.8", "0.4/0/0.6", "0.6/0/0.4", "0.8/0/0.2","0.9/0.1/0","0.7/0.3/0", "0.2/0.05/0.75"])

fig2, (ax3, ax4) = plt.subplots(1, 2, constrained_layout=True)
sns.barplot(ax=ax3, x="variable", y="value",
            data=pd.melt(df_lock_free_var), capsize=.2, errwidth=1)
ax3.set(xlabel="Load factor (add/remove/find)", ylabel='ops/msec')

sns.barplot(ax=ax4, x="variable", y="value",
            data=pd.melt(df_lock_based_var), capsize=.2, errwidth=1)
ax4.set(xlabel="Load factor (add/remove/find)", ylabel='ops/msec')

upper_ylimit_var = 150000
ax3.set_ylim([0, upper_ylimit_var])
ax4.set_ylim([0, upper_ylimit_var])

ax3.set_title('Lock-free Hash Table')
ax4.set_title('Lock-based Hash Table')

ax3.tick_params(axis='x', rotation=45)
ax4.tick_params(axis='x', rotation=45)

plt.show()
