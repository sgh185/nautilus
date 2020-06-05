#!/home/sgi9754/wllvm-test/bin/python

# Run all tests together

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

