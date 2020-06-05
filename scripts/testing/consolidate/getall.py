#!/home/sgi9754/wllvm-test/bin/python

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
		subprocess.call('./scripts/testing/getdata.py {} fiberbench{} {}'.format(sys.argv[1], i, sys.argv[2]), shell=True)
	except:
		print(str(i) + " failed")

