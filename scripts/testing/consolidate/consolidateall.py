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

bench = range(3, 21)
bench.append(100)
print bench
for i in bench:
	try:
		subprocess.call('./scripts/testing/consolidate.py fiberbench{}'.format(i), shell=True)
	except:
		print(str(i) + " failed")

