#!/home/sgi9754/wllvm-test/bin/python

# Generate line mashed plots for all
# benchmarks in one figure

import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import os
import re
import subprocess

benches = [3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]

bench = sys.argv[1]
grans = [200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000]
names = ["Dot Product", "Linked List Traversal", "Matrix Multiply", "Binary Search Tree", "Randomized Matrix Multiply", "Level Order Tree Traversal", "Randomized FP Ops", "DNE", "Rijndael", "MD5", "SHA-1", "Randomized Fibonacci Computation", "KNN Classifier", "Cycle Detection", "Unweighted MST", "Dijkstra's Algorithm", "Quick Sort", "Radix Sort", "Streamcluster"]
allint = {}
allover = {}

def getAllData():
	fiberd = getFiberData(sys.argv[1])

# Get data sets from print_data()
def getFiberData(fileName):
	query = bench.upper()
	start = query + "START"
	end = query + "END"
	# print start
	# print end
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

def getAllMeans(b):
    with open(sys.argv[1]) as file:
        data = re.findall('FIBERBENCH{}START(.*?)FIBERBENCH{}END'.format(b, b), file.read(), re.DOTALL)
        data = data[0].split() 
        return data


def createMashPlot(the_data):
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111)
    # ax.set_xticklabels([0, 200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000], fontsize=16)

    for b in benches:
        d = [float(elm) for elm in the_data[b]]
        ax.plot(grans, d, marker='o')
   
    plt.xticks(rotation='50')
    plt.yscale('log')
    plt.xscale('log')
    # plt.tick_params(axis='both', which='minor')
    ax.get_xaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter()) 
    
    # plt.setp(ax.yaxis.get_minorticklabels(), rotation='20')
    # plt.setp(ax.xaxis.get_minorticklabels(), rotation='50')

    ax.get_yaxis().set_major_locator(plt.FixedLocator([200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000]))
    ax.get_xaxis().set_major_locator(plt.FixedLocator([200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000]))
    ax.get_yaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter()) 
    ax.get_yaxis().set_major_formatter(matplotlib.ticker.ScalarFormatter()) 
    # ax.xaxis.set_minor_formatter(matplotlib.ticker.FormatStrFormatter("%d"))
    # ax.yaxis.set_minor_formatter(matplotlib.ticker.FormatStrFormatter("%d"))
    bottom, top = plt.ylim()
    if (int(sys.argv[3])): plt.ylim(bottom, 26000)
    plt.yticks(fontsize=16)
    plt.xticks(fontsize=16)
    # plt.yticks([0, 200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000], fontsize=16)

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
    plt.title(sys.argv[2], fontsize=24)
    plt.xlabel('Granularity (Cycles, log scale)', fontsize=24, labelpad=15)
    plt.ylabel('Cost (Cycles, log scale)', fontsize=24, labelpad=15)
    plt.savefig(sys.argv[2] + '-All.pdf', bbox_inches='tight')
    return

# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]
'''
getAllData()
print "FIBERBENCH{}START".format(sys.argv[2])
createBoxPlot(allint, "Intervals")
# createBoxPlot(allover, "Overheads")
print "FIBERBENCH{}END".format(sys.argv[2])
'''

all_data = {}
for b in benches:
    all_data[b] = getAllMeans(b)


print(all_data)

createMashPlot(all_data)




