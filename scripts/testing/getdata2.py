#!/home/sgi9754/wllvm-test/bin/python

import os
import sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

folders = ["200", "400", "600", "800", "1000"]
name = sys.argv[1]
data = "data/"
# Convert everything to ints --- removes any potential strings, for future analysis
def getIntList(data):
	intList = []
	for item in data:
		try:
			intList.append(abs(int(item)))
		except Exception:
			pass

	return intList

# Transform the array to weed out outliers (@eumiro --- SO)
def transformNoOutliers(data, M=2):
	return data[abs(data - np.mean(data)) < M * np.std(data)]

def getMeanDataSet(filename):
	means = {}
	for folder in folders:
		try:
			lines = [line.rstrip('\n') for line in open(data + folder + '/' + filename)]
			means[int(folder)] = (np.mean(transformNoOutliers(np.array(getIntList(lines)))))
		except Exception:
			pass
	
	print means
	return means

def createPlot(means):
	x = np.array(list(means.keys()))
	y = np.array(list(means.values()))

	# plt.plot(x, y, linestyle='--', marker='o')
	plt.scatter(x, y, marker='o')
	plt.plot(np.unique(x), np.poly1d(np.polyfit(x, y, 1))(np.unique(x)), linestyle='--')
	m, b = np.poly1d(np.polyfit(x, y, 1))
	plt.plot([], [], ' ', label=("Slope: " + str(m)))
	plt.plot([], [], ' ', label=("Intercept: " + str(b)))
	plt.legend()
	plt.xlabel("Granularity")
	plt.ylabel("Mean interval size (between yields)")
	plt.savefig(name + "line.png")	
	return;

means = getMeanDataSet(name)
createPlot(means)





