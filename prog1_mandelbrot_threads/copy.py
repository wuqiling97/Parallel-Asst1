import os, sys
import re


if 'r' in sys.argv:
	build = 'Release'
else:
	build = 'Debug'
print('copy', build)

path = os.path.abspath('.')
res = re.search('(?<=prog)\d', path).group()
exe = r'..\x64\{}\proj{}.exe'.format(build, res)
os.system('copy {} .'.format(exe))
