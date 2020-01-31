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
	with open('serial.out') as file:
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
def createPlot(data, name, xLabel, title, gran):
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
	
	plt.xlabel(xLabel)
	plt.ylabel('Frequency')
	plt.legend()

	plt.title(title)

	plt.savefig('data/time-hook/{}/{}.png'.format(gran, name))
	
	return

def getCombinedMedians(y, q, f, g):
	yield_d = transformNoOutliers(np.array(transformNoOutliers(np.array(transformDataSets(y)))))
	queue = transformNoOutliers(np.array(transformDataSets(q))) if g == 1000 else transformNoOutliers(np.array(transformNoOutliers(np.array(transformDataSets(q)))))
	fire = transformNoOutliers(np.array(transformNoOutliers(np.array(transformDataSets(f)))))
	return np.median(np.array(yield_d)), np.median(np.array(queue)), np.median(np.array(fire))

def generateLinePlot(g, q, f, y):
	plt.scatter(g, q, marker='o')
	plt.scatter(g, f, marker='o')
	plt.scatter(g, y, marker='o')
	d = [i - j for i, j in zip(f, y)]
	plt.scatter(g, d, marker='o')
	
	
	m, b = np.poly1d(np.polyfit(g, q, 1))
	plt.plot(np.unique(g), np.poly1d(np.polyfit(g, q, 1))(np.unique(g)), linestyle='--', label=("Queueing, m: " + str(round(m, 2)) + ", b: " + str(round(b, 2))))
	m, b = np.poly1d(np.polyfit(g, f, 1))
	plt.plot(np.unique(g), np.poly1d(np.polyfit(g, f, 1))(np.unique(g)), linestyle='--', label=("Firing, m: " + str(round(m, 2)) + ", b: " + str(round(b, 2))))

	m, b = np.poly1d(np.polyfit(g, y, 1))
	plt.plot(np.unique(g), np.poly1d(np.polyfit(g, y, 1))(np.unique(g)), linestyle='--', label=("Yielding, m: " + str(round(m, 2)) + ", b: " + str(round(b, 2))))
	
	# Difference between f and y
	m, b = np.poly1d(np.polyfit(g, d, 1))
	plt.plot(np.unique(g), np.poly1d(np.polyfit(g, d, 1))(np.unique(g)), linestyle='--', label=("Firing - yielding, m: " + str(round(m, 2)) + ", b: " + str(round(b, 2))))
		

	plt.legend(fontsize=8)
	plt.xlabel("Granularity")
	plt.ylabel("Mean time (in clock cycles, per category)")
	plt.savefig("data/time-hook/THline.png")	
	
	return


if __name__ == "__main__":
	option = sys.argv[1]
	
	if option == '-t':
		gran = sys.argv[2]
		
		if not os.path.exists('data/time-hook/{}/'.format(gran)):
			os.makedirs('data/time-hook/{}/'.format(gran))
		
		# generate histograms
		
		first, second = getTHDataSets()
		first = transformDataSets(first)
		second = transformDataSets(second)
		print(first)
		print(second)
		total_overhead = [f + s for f, s in zip(first, second)]
		createPlot(total_overhead, 'TH_total_{}'.format(gran), "Total overhead --- nk_time_hook_fire (clock cycles)", "Total overhead nk_time_hook_fire --- {}".format(gran), gran) 
		createPlot(first, 'TH_queue_{}'.format(gran), "Time to queue --- nk_time_hook_fire (clock cycles)", "Queueing in nk_time_hook_fire --- {}".format(gran), gran) 
		createPlot(second, 'TH_fire_{}'.format(gran), "Time to fire --- nk_time_hook_fire (clock cycles)", "Firing in nk_time_hook_fire --- {}".format(gran), gran) 

		first = transformNoOutliers(np.array(first))
		second = transformNoOutliers(np.array(second))
		total_overhead = [f + s for f, s in zip(first, second)]
		createPlot(total_overhead, 'TH_total_{}_one_pass'.format(gran), "Total overhead --- nk_time_hook_fire (clock cycles)", "Total overhead nk_time_hook_fire --- {}".format(gran), gran) 
		createPlot(first, 'TH_queue_{}_one_pass'.format(gran), "Time to queue --- nk_time_hook_fire (clock cycles)", "Queueing in nk_time_hook_fire --- {}".format(gran), gran) 
		createPlot(second, 'TH_fire_{}_one_pass'.format(gran), "Time to fire --- nk_time_hook_fire (clock cycles)", "Firing in nk_time_hook_fire --- {}".format(gran), gran) 

		first = transformNoOutliers(np.array(first))
		second = transformNoOutliers(np.array(second))
		createPlot(first, 'TH_queue_{}_two_pass'.format(gran), "Time to queue --- nk_time_hook_fire (clock cycles)", "Queueing in nk_time_hook_fire --- {}".format(gran), gran) 
		createPlot(second, 'TH_fire_{}_two_pass'.format(gran), "Time to fire --- nk_time_hook_fire (clock cycles)", "Firing in nk_time_hook_fire --- {}".format(gran), gran) 
		total_overhead = [f + s for f, s in zip(first, second)]
		createPlot(total_overhead, 'TH_total_{}_two_pass'.format(gran), "Total overhead --- nk_time_hook_fire (clock cycles)", "Total overhead nk_time_hook_fire --- {}".format(gran), gran) 




		# move serial file
		subprocess.call('cp serial.out data/time-hook/{}/serialTH{}.out'.format(gran, gran), shell=True)
	elif option == '-c':
		grans = [200, 400, 600, 800, 1000]
		queueing_data = []
		firing_data = []
		yielding_data = []
		for gran in grans:
			str_gran = str(gran)
			fname = 'serialTH{}.out'.format(str_gran)
			yield_data, queue_data, fire_data = getCombinedDataSets(fname)
			yield_med, queue_med, fire_med = getCombinedMedians(yield_data, queue_data, fire_data, gran)
			yielding_data.append(yield_med)
			queueing_data.append(queue_med)
			firing_data.append(fire_med)

		generateLinePlot(grans, queueing_data, firing_data, yielding_data)












