#!/home/sgi9754/wllvm-test/bin/python

# This shitty script analyzes a single fiber benchmark
# data set from a serial output. Original data set is preserved
# as the benchmark name. Plot for original data is saved as 
# [benchmark-name].png. One pass outlier removal, two pass outlier
# removal and plots for all are generated. File names are self
# explanatory. 

import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import os
import re
import subprocess

name = "data/" + sys.argv[1] + "/" + sys.argv[2]
fileName = sys.argv[3]

subprocess.call('cp {} {}-raw'.format(fileName, name), shell=True)

# Get data sets from print_data()
def getDataSets():
	with open(fileName) as file:
    		return re.findall('PRINTSTART(.*?)PRINTEND', file.read(), re.DOTALL)

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
		transformed.append(getIntList(string.splitlines())[2:])
		# reasoning for [2:-3] --- first rdtsc is usually bad, second one
		# is subtracting from 0.
	
	joinedTransformed = []
	for transform in transformed:
		joinedTransformed.extend(transform)
	
	return joinedTransformed


# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]

# Create figures, save figure as png, save data as file
def createPlot(data, refined=False, doubleRefined=False):
	binNum = 200
	nTransformed = np.array(data)
	newName = name
	granularity = int(sys.argv[1])

	if refined:
		nTransformed = transformNoOutliers(np.array(data))
		binNum = 80
		newName = name + "no-out-one-pass"
		if doubleRefined:
			nTransformed = transformNoOutliers(np.array(nTransformed))
			newName = name + "no-out-two-pass"
	
	# Save figure from matplotlib
	# FULL DATA
	plt.figure(figsize=(10, 8))
	plt.hist(nTransformed, normed=False, bins=binNum)

	mean = np.mean(nTransformed)
	median = np.median(nTransformed)
	variance = np.var(nTransformed)
	stdev = np.std(nTransformed)
	maxN = max(nTransformed)
	minN = min(nTransformed)

	plt.plot([], [], ' ', label=("Mean: " + str(mean)))
	plt.plot([], [], ' ', label=("Median: " + str(median)))
	plt.plot([], [], ' ', label=("Variance: " + str(variance)))
	plt.plot([], [], ' ', label=("StDev: " + str(stdev)))
	plt.plot([], [], ' ', label=("Maximum: " + str(maxN)))
	plt.plot([], [], ' ', label=("Minimum: " + str(minN)))
	
	plt.xlabel('Interval between yield calls (cycles)', fontsize=12)
	plt.ylabel('Frequency', fontsize=12)
	plt.legend(fontsize=8)
	
	plt.savefig(newName + '.png')

	# Save data to file
	np.savetxt(newName, nTransformed, delimiter=" ", fmt='%s')

	# OVERHEAD DATA

	nTransformed = [i - granularity for i in nTransformed] 

	plt.figure(figsize=(10, 8))
	plt.hist(nTransformed, normed=False, bins=binNum)

	mean = np.mean(nTransformed)
	median = np.median(nTransformed)
	variance = np.var(nTransformed)
	stdev = np.std(nTransformed)
	maxN = max(nTransformed)
	minN = min(nTransformed)

	plt.plot([], [], ' ', label=("Mean: " + str(mean)))
	plt.plot([], [], ' ', label=("Median: " + str(median)))
	plt.plot([], [], ' ', label=("Variance: " + str(variance)))
	plt.plot([], [], ' ', label=("StDev: " + str(stdev)))
	plt.plot([], [], ' ', label=("Maximum: " + str(maxN)))
	plt.plot([], [], ' ', label=("Minimum: " + str(minN)))
	
	plt.xlabel('Absolute Overhead (cycles)', fontsize=12)
	plt.ylabel('Frequency', fontsize=12)
	plt.legend(fontsize=8)
	
	plt.savefig(newName + '-overhead.png')

	# Save data to file
	np.savetxt(newName, nTransformed, delimiter=" ", fmt='%s')
	
	return


rawData = getDataSets()
transformedData = transformDataSets(rawData)
createPlot(transformDataSets(rawData))
createPlot(transformDataSets(rawData), True)
createPlot(transformDataSets(rawData), True, True)

if not os.path.exists('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2])):
	os.makedirs('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2]))

subprocess.call('mv data/{}/{}* data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2], sys.argv[1], sys.argv[2]), shell=True)

