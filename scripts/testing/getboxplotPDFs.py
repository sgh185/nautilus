#!/home/mac4602/wllvmnaut/bin/python

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
plt.rcParams.update({'font.size': 20})
import matplotlib.mlab as mlab
import os
import re
import subprocess

#if sys.argv[1] == "yield":
#    if sys.argv[1] == "fpr":
#        fileName = "./nohyp-fp-fibertiming-trial1.out"
#    else:
#        fileName = "./nohyp-nofp-fibertiming-trial1.out"
#elif sys.argv[1] == "fpr":
#    fileName = "./fib_fpr_creation.out"
#else:
#    fileName = "./fib_no_fpr_creation.out"
yieldType = sys.argv[1]
machine = sys.argv[2]
fileName = sys.argv[3]

#subprocess.call('cp {} {}-raw'.format(fileName, fileName), shell=True)

# Get data sets from print_data()
def getDataSets():
    startParsing = False
    dataOut = []
    flines = open(fileName).readlines()
    for l in flines:
        if "PRINTSTART" in l:
            startParsing = True
            continue
        if "PRINTEND" in l:
            startParsing = False
            break
        if startParsing:
            dataOut.append(l)
    return dataOut
    		#return re.findall(r'PRINTSTART(.*?)PRINTEND', file.read(), re.DOTALL)

# Convert everything to ints --- removes any potential strings, for future analysis
def getIntList(data):
    intSaveList = []
    intYieldList = []
    intRestoreList = []
    intTotalList = []
    d = data[:]
    for x in d:
        splitStr = x.split()
        try:
            intSaveList.append(abs(int(splitStr[0])))
        except Exception:
            continue
        try:
            intYieldList.append(abs(int(splitStr[1])))
        except Exception:
            continue
        try:
            intRestoreList.append(abs(int(splitStr[2])))
        except Exception:
            continue
        try:
            intTotalList.append(abs(int(splitStr[3])))
        except Exception:
            continue

    
    return intSaveList, intYieldList, intRestoreList, intTotalList

def getIntCreationList(data):
    intCreationList = []
    d = data[:]
    for x in d:
        splitStr = x.split()
        try:
            intCreationList.append(abs(int(splitStr[0])))
        except Exception:
            continue
    
    return intCreationList


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
def transformNoOutliers(data, M=3):
	return np.array(data[abs(data - np.mean(data)) < M * np.std(data)])

# Create figures, save figure as pdf, save data as file
def createPlot(data, nameOfFile, nameOfData, binNum):
    nTransformed = data
    newName = "new" + fileName

	#if refined:
	#	nTransformed = transformNoOutliers(np.array(data))
	#	binNum = 80
	#	newName = name + "no-out-one-pass"
	#	if doubleRefined:
	#		nTransformed = transformNoOutliers(np.array(nTransformed))
	#		newName = name + "no-out-two-pass"
	
	# Save figure from matplotlib
	# FULL DATA
    plt.figure(figsize=(10, 8))
    plt.hist(nTransformed, normed=False, bins=binNum)
    plt.title("Histogram - " + nameOfData)

    dataLen = np.shape(nTransformed)
    mean = np.mean(nTransformed)
    median = np.median(nTransformed)
    variance = np.var(nTransformed)
    stdev = np.std(nTransformed)
    maxN = max(nTransformed)
    minN = min(nTransformed)

    plt.plot([], [], ' ', label=("N: " + str(dataLen[0])))
    plt.plot([], [], ' ', label=("Mean: " + str(round(mean,4))))
    plt.plot([], [], ' ', label=("Median: " + str(median)))
    plt.plot([], [], ' ', label=("StDev: " + str(round(stdev,4))))
    plt.plot([], [], ' ', label=("Max: " + str(maxN) + "  Min: " + str(minN)))	
    
    plt.xlabel('Cost of ' + nameOfData, fontsize=24)
    plt.ylabel('Frequency', fontsize=24)
    plt.legend(fontsize=16)	

    plt.savefig(nameOfFile + '.pdf', bbox_inches='tight')

	# Save data to file
    np.savetxt(nameOfFile, nTransformed, delimiter=" ", fmt='%s')

    return
	# OVERHEAD DATA
"""
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
"""	

# Create Box Plot figures, save figure as pdf, save data as file
def createBoxPlot(sData, yData, rData, tData, nameOfFile, nameOfData):
    newName = "new" + fileName

		
	# Save figure from matplotlib
	# FULL DATA
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111)
    dataList = [tData, sData, yData, rData]
    bp = ax.boxplot(dataList, showmeans=True, whis=5, showbox=True, meanline=True)
    ax.set_xticklabels(["Total", "Saving", "Switching", "Restoring"])
    plt.title("Boxplot - " + nameOfData)

    dataLen = np.shape(tData)
    mean = np.mean(tData)
    median = np.median(tData)
    variance = np.var(tData)
    stdev = np.std(tData)
    maxN = max(tData)
    minN = min(tData)

    plt.plot([], [], ' ', label=("N: " + str(dataLen[0])))
    plt.plot([], [], ' ', label=("Mean: " + str(round(mean,4))))
    plt.plot([], [], ' ', label=("Median: " + str(median)))
    plt.plot([], [], ' ', label=("StDev: " + str(round(stdev,4))))
    plt.plot([], [], ' ', label=("Max: " + str(maxN) + "  Min: " + str(minN)))	
    
    plt.xlabel('Phase', fontsize=24)
    plt.ylabel('Cost (Cycles)', fontsize=24)
    plt.legend(fontsize=16)	

    plt.savefig(nameOfFile + '.pdf', bbox_inches='tight')

	# Save data to file
    #np.savetxt(nameOfFile, nTransformed, delimiter=" ", fmt='%s')

    return
	# OVERHEAD DATA
"""
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
"""	

def createScatterPlot(data, nameOfFile, nameOfData):
    nTransformed = data
    newName = "new" + fileName

	#if refined:
	#	nTransformed = transformNoOutliers(np.array(data))
	#	binNum = 80
	#	newName = name + "no-out-one-pass"
	#	if doubleRefined:
	#		nTransformed = transformNoOutliers(np.array(nTransformed))
	#		newName = name + "no-out-two-pass"
	
	# Save figure from matplotlib
	# FULL DATA
    plt.figure(figsize=(14, 10))
    xVals = np.arange(1,np.shape(nTransformed)[0]+1)
    plt.scatter(xVals, nTransformed)
    plt.title("Scatter Plot - " + nameOfData)

    dataLen = np.shape(nTransformed)
    mean = np.mean(nTransformed)
    median = np.median(nTransformed)
    variance = np.var(nTransformed)
    stdev = np.std(nTransformed)
    maxN = max(nTransformed)
    minN = min(nTransformed)

    plt.plot([], [], ' ', label=("N: " + str(dataLen[0])))
    plt.plot([], [], ' ', label=("Mean: " + str(round(mean,4))))
    plt.plot([], [], ' ', label=("Median: " + str(median)))
    plt.plot([], [], ' ', label=("StDev: " + str(round(stdev,4))))
    plt.plot([], [], ' ', label=("Max: " + str(maxN) + "  Min: " + str(minN)))	
	
    plt.ylabel('Cost of ' + nameOfData, fontsize=24)
    plt.xlabel('Yield Number', fontsize=24)
    plt.legend(fontsize=16)
	
    plt.savefig("scatter-" + nameOfFile + '.png', bbox_inches='tight')

	# Save data to file
    np.savetxt(nameOfFile, nTransformed, delimiter=" ", fmt='%s')

    return
	


rawData = getDataSets()
sList, yList, rList, tList = getIntList(rawData)

# List of reg saving costs
sListNoOut = sList[:]
sListNoOut = np.array(sListNoOut)
sListNoOut = transformNoOutliers(sListNoOut, M=3)
sList = np.array(sList)
sMean = np.mean(sList)
print("Save Mean: " + str(sMean) + "\n")
sMeanNoOut = np.mean(sListNoOut)
print("Save Mean (no outliers): " + str(sMeanNoOut) + "\n")

# List of yield (scheduling) costs
yListNoOut = yList[:]
yListNoOut = np.array(yListNoOut)
yListNoOut = transformNoOutliers(yListNoOut, M=3)
yList = np.array(yList)
yMean = np.mean(yList)
print("Yield Mean: " + str(yMean) + "\n")
yMeanNoOut = np.mean(yListNoOut)
print("Yield Mean (no outliers): " + str(yMeanNoOut) + "\n")

# List of reg restore costs
rListNoOut = rList[:]
rListNoOut = np.array(rListNoOut)
rListNoOut = transformNoOutliers(rListNoOut, M=3)
rList = np.array(rList)
rMean = np.mean(rList)
print("Restore Mean: " + str(rMean) + "\n")
rMeanNoOut = np.mean(rListNoOut)
print("Restore Mean (no outliers): " + str(rMeanNoOut) + "\n")

# List of total yield costs
tListNoOut = tList[:]
tListNoOut = np.array(tListNoOut)
tListNoOut = transformNoOutliers(tListNoOut, M=3)
tList = np.array(tList)
tMean = np.mean(tList)
print("Total Yield Mean: " + str(tMean) + "\n")
tMeanNoOut = np.mean(tListNoOut)
print("Total Yield Mean (no outliers): " + str(tMeanNoOut) + "\n")

outliers = 0
for i in yList:
    if i > 2000:
        outliers += 1

for i in sList:
    if i > 2000:
        outliers += 1

for i in rList:
    if i > 2000:
        outliers += 1

for i in tList:
    if i > 2000:
        outliers += 1

print(outliers)
    
#transformedData = transformDataSets(rawData)

#if sys.argv[2] == "yield":
createBoxPlot(sList, yList, rList, tList, machine + "-" + yieldType + "-yieldBoxPlot", "Cost of " + yieldType + " Yield on " + machine)
createBoxPlot(sListNoOut, yListNoOut, rListNoOut, tListNoOut, machine + "-" + yieldType + "-NoOut-yieldBoxPlot", "Cost of " + yieldType + " Yield on " + machine)
#createBoxPlot(sList, yList, rList, tList, "yieldBoxPlot-NOFPR", "Cost of " yieldType + " Yield on " + machine)

#if not os.path.exists('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2])):
#	os.makedirs('data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2]))

#subprocess.call('mv data/{}/{}* data/{}/dir-{}/'.format(sys.argv[1], sys.argv[2], sys.argv[1], sys.argv[2]), shell=True)

