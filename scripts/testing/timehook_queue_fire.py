#!/home/sgi9754/wllvm-test/bin/python

import os
import sys
import re
import numpy as np
import subprocess
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab

# Collect timing data for time-hook interface --- gather cycle counts
# for setup and queueing portion of nk_time_hook_fire and for execution
# of the registered hook functions (two separate data sets). Data is 
# collected against different granularities (200 cycles --- 1000 cycles)
# and plotted. Benchmarks used are in fiber_benchmark.c (command in 
# Nautilus is 'timehook1'.


# Get data sets from get_time_hook_data()
def getTHDataSets():
	with open('comptiming64.out') as file:
		first_data = re.findall('th_one_start(.*?)th_one_end', file.read(), re.DOTALL)
		file.seek(0)
		second_data = re.findall('th_two_start(.*?)th_two_end', file.read(), re.DOTALL)
		return first_data, second_data

def getCombinedDataSets(fname):
	with open(fname) as file:
		yield_data = re.findall('PRINTSTART(.*?)PRINTEND', file.read(), re.DOTALL)
		file.seek(0)
		first_data = re.findall('th_one_start(.*?)th_one_end', file.read(), re.DOTALL)
		file.seek(0)
		second_data = re.findall('th_two_start(.*?)th_two_end', file.read(), re.DOTALL)
		return yield_data, first_data, second_data

# Convert everything to ints --- removes any potential strings, for future analysis
def getIntList(data):
	intList = []
	for item in data:
		try:
			intList.append(abs(int(item)))
		except Exception:
			pass

	return intList

def transformDataSets(data):
	# Current data sets pulled from qemu --- caveats:
	# Ignore first 3 data sets
	# Ignore first and last 3 entries in each set
	# ^ Qemu cycle counters are thrown off at these points (possibly ???)
	transformed = []
	for string in data:
		transformed.append(getIntList(string.splitlines()))
	
	joinedTransformed = []
	for transform in transformed:
		joinedTransformed.extend(transform)
	
	return joinedTransformed

# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]

# Create figures, save figure as png, save data as file
def createPlot(data, name, title):
	binNum = 20
	
	# Save figure from matplotlib
	plt.figure(figsize=(10, 8))
	plt.hist(data, normed=False, bins=binNum)

	mean = np.mean(data)
	median = np.median(data)
	variance = np.var(data)
	stdev = np.std(data)
	maxN = max(data)
	minN = min(data)

	plt.plot([], [], ' ', label=("Mean: " + str(mean)))
	plt.plot([], [], ' ', label=("Median: " + str(median)))
	plt.plot([], [], ' ', label=("Variance: " + str(variance)))
	plt.plot([], [], ' ', label=("StDev: " + str(stdev)))
	plt.plot([], [], ' ', label=("Maximum: " + str(maxN)))
	plt.plot([], [], ' ', label=("Minimum: " + str(minN)))
	
	plt.xlabel("Time (clock cycles)")
	plt.ylabel('Frequency')
	plt.legend()

	plt.title(title)

	plt.savefig('./{}.png'.format(name))
	# plt.savefig('data/time-hook/{}/{}.png'.format(gran, name))
	
	return

first, second = getTHDataSets()
first = transformDataSets(first)
second = transformDataSets(second)
print(first)
first_noout = transformNoOutliers(np.array(first))
second_noout = transformNoOutliers(np.array(second))


createPlot(first, "queue64", "Queueing64")
createPlot(first_noout, "queue-noout64", "Queueing64 - No outliers")
createPlot(second, "fire64", "Firing64")
createPlot(second_noout, "fire-noout64", "Firing64 - No outliers")
