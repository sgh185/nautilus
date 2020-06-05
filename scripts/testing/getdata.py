#!/home/sgi9754/wllvm-test/bin/python

# This script analyzes a single fiber benchmark
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
def getFiberData():
	query = sys.argv[2].upper()
	start = query + "START"
	end = query + "END"
	print start
	print end
	with open(fileName) as file:
			data = re.findall('{}(.*?){}'.format(start, end), file.read(), re.DOTALL)
			return ''.join([elm for elm in data])

def getDataSets(data):
		return re.findall('PRINTSTART(.*?)PRINTEND', data, re.DOTALL)

def getOverheadSets(data):
    return re.findall('OVERHEADSTART(.*?)OVERHEADEND', data, re.DOTALL)

def getEarlyCounts(data):
    return re.findall('EARLYSTART(.*?)EARLYEND', data, re.DOTALL)

def getLateCounts(data):
    return re.findall('LATESTART(.*?)LATEEND', data, re.DOTALL)

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
	binNum = 32 
	nTransformed = np.array(data)
	newName = name
	granularity = int(sys.argv[1])

	if refined:
		nTransformed = transformNoOutliers(np.array(data))
		binNum = 64
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
	
	plt.xlabel('Interval between calls to hook function (clock cycles)', fontsize=12)
	plt.ylabel('Frequency', fontsize=12)
	plt.legend(fontsize=8)
	
	plt.savefig(newName + '.png')

	# Save data to file
	np.savetxt(newName, nTransformed, delimiter=" ", fmt='%s')
	retmean = mean
	retmedian = median

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
	
	return retmedian, retmean

def createOverheadPlots(overheads, intmed, intmean):
	binNum = 32 
	nTransformed = np.array(overheads)
	newName = name + "-overheads"
	granularity = int(sys.argv[1])

	# Save figure from matplotlib
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
	plt.plot([], [], ' ', label=("% Overhead (med:obs): " + str(round(100 * (float(median) / float(intmed)), 3))))
	plt.plot([], [], ' ', label=("% Overhead (avg:obs): " + str(round(100 * (float(mean) / float(intmean)), 3))))
	plt.plot([], [], ' ', label=("% Overhead (med:tgt): " + str(round(100 * (float(median) / float(granularity)), 3))))
	plt.plot([], [], ' ', label=("% Overhead (avg:tgt): " + str(round(100 * (float(mean) / float(granularity)), 3))))

	plt.xlabel('Processing overhead (between) calls to hook function (clock cycles)', fontsize=12)
	plt.ylabel('Frequency', fontsize=12)
	plt.legend(fontsize=8)
	
	plt.savefig(newName + '.png')

	# Save data to file
	np.savetxt(newName, nTransformed, delimiter=" ", fmt='%s')

	return


def createOverheadCountPlots(early, late):
	plt.figure(figsize=(10, 8))

	for i in range(len(early)):
		early[i] = int(early[i])
		late[i] = int(late[i])
	
	totalEarly = sum(early)
	totalLate = sum(late)
	ratio = float(totalEarly) / float(totalLate) 
	data = [totalEarly, totalLate]

	categories = ("Early", "Late")
	indices = np.arange(2)
	plt.bar(indices, data) 
	plt.xticks(indices, categories)

	plt.plot([], [], ' ', label=("Early: " + str(totalEarly)))
	plt.plot([], [], ' ', label=("Late/On-time: " + str(totalLate)))
	plt.plot([], [], ' ', label=("Ratio (early : late): " + str(ratio)))
	
	plt.xlabel('Overhead --- Early vs. Late Calls', fontsize=12)
	plt.ylabel('Frequency', fontsize=12)
	plt.legend(fontsize=8)
	
	plt.savefig(name + '-early-late.png')

	return

fiberData = getFiberData()
rawData = getDataSets(fiberData)
transformedData = transformDataSets(rawData)
overheadData = transformDataSets(getOverheadSets(fiberData))
median, mean = createPlot(transformDataSets(rawData))
createPlot(transformDataSets(rawData), True)
createPlot(transformDataSets(rawData), True, True)
createOverheadPlots(overheadData, median, mean)

if not os.path.exists('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2])):
	os.makedirs('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2]))

subprocess.call('mv data/{}/{}* data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2], sys.argv[1], sys.argv[2]), shell=True)

