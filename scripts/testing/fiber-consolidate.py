#!/home/mac4602/wllvmnaut/bin/python

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

bench = sys.argv[1]
grans = [200, 400, 600, 800, 1000, 2000, 4000, 8000]# 16000]
names = ["Dot Product", "Linked List Traversal", "Matrix Multiply", "Binary Search Tree", "Randomized Matrix Multiply", "Level Order Tree Traversal", "Randomized FP Ops", "DNE", "Rijndael", "MD5", "SHA-1", "Randomized Fibonacci Computation", "KNN Classifier", "Cycle Detection", "Unweighted MST", "Dijkstra's Algorithm", "Quick Sort", "Radix Sort", "Streamcluster"]
namenum = int(sys.argv[2])-3
allint = {}
allover = {}

def getAllData():
	for gran in grans:
		fiberd = getFiberData('dataFPYield/{}/fiberbench10-raw'.format(gran))
		rawd = getDataSets(fiberd)
		transformedd = transformDataSets(rawd)
		overheadd = transformDataSets(getOverheadSets(fiberd))
		allint[gran] = [x for x in transformedd if x < 30000]
		allover[gran] = [x for x in overheadd if x < 30000]
		print allover[gran]
		print allint[gran]

def getAllRefined():
	for gran in grans:
		fiberd = getFiberData('dataFPYield/{}/fiberbench10-raw'.format(gran))
		rawd = getDataSets(fiberd)
		transformedd = transformDataSets(rawd)
		overheadd = transformDataSets(getOverheadSets(fiberd))
		allint[gran] = transformedd
		allover[gran] = overheadd

# Get data sets from print_data()
def getFiberData(fileName):
	query = bench.upper()
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
		transformed.append(getIntList(string.splitlines())[2:-2])
		# reasoning for [2:-3] --- first rdtsc is usually bad, second one
		# is subtracting from 0.
	
	joinedTransformed = []
	for transform in transformed:
		joinedTransformed.extend(transform)
	
	return joinedTransformed

def createBoxPlot(allofit, t):
    global namenum
    dataToPlot = [allofit[gran] for gran in grans]
    #dataToPlot = np.array(dataToPlot) 
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111)
    bp = ax.boxplot(dataToPlot, showmeans=True, whis=5, showbox=True)
    ax.set_xticklabels(grans, fontsize=16)
    plt.yticks(fontsize=16)
    #plt.title(t + "-{}".format(bench))
    plt.title(t + " Boxplot - " + names[namenum], fontsize=24)
    '''
    dataLen = np.shape(dataToPlot)
    mean = np.mean(dataToPlot)
    median = np.median(dataToPlot)
    variance = np.var(dataToPlot)
    stdev = np.std(dataToPlot)
    maxN = max(dataToPlot)
    minN = min(dataToPlot)
    plt.plot([], [], ' ', label=("N: " + str(dataLen[0])))
    plt.plot([], [], ' ', label=("Mean: " + str(round(mean,4))))
    plt.plot([], [], ' ', label=("Median: " + str(median)))
    plt.plot([], [], ' ', label=("StDev: " + str(round(stdev,4))))
    plt.plot([], [], ' ', label=("Max: " + str(maxN) + "  Min: " + str(minN)))
    '''
    plt.xlabel('Granularity (Cycles)', fontsize=24, labelpad=15)
    plt.ylabel('Cost (Cycles)', fontsize=24, labelpad=15)
    plt.savefig(bench + t + '-Boxplot.pdf', bbox_inches='tight')
    return

# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]

getAllData()
createBoxPlot(allint, "Intervals")
createBoxPlot(allover, "Overheads")

