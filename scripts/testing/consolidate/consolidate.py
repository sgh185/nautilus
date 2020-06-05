#!/home/sgi9754/wllvm-test/bin/python

# Generate fiberbench data

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
grans = [200, 400, 600, 800, 1000, 2000, 4000, 8000, 16000]
allint = {}
allover = {}

def getAllData():
	for gran in grans:
		fiberd = getFiberData('data/{}/fiberbench10-raw'.format(gran))
		rawd = getDataSets(fiberd)
		transformedd = transformDataSets(rawd)
		overheadd = transformDataSets(getOverheadSets(fiberd))
		allint[gran] = transformedd
		allover[gran] = overheadd
		print allover[gran]
		print allint[gran]

def getAllRefined():
	for gran in grans:
		fiberd = getFiberData('data/{}/fiberbench10-raw'.format(gran))
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
	dataToPlot = [allofit[gran] for gran in grans] 
	fig = plt.figure(figsize=(10, 8))
	ax = fig.add_subplot(111)
	bp = ax.boxplot(dataToPlot, showmeans=True, whis=5, showbox=True)
	ax.set_xticklabels(grans)
	plt.title(t + "-{}".format(bench))
	plt.savefig(bench + t + '.pdf', bbox_inches='tight')
	return

# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]

getAllData()
createBoxPlot(allint, "Intervals-Boxplot")
createBoxPlot(allover, "Overheads-Boxplot")

