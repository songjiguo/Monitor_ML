#!/usr/bin/python
## script helps to maintain two different version of code base
## 1. normal base
## 2. recovery base
##    able to choose the combination of above two

import os
import sys

names = ['mem_mgr','torrent', 'sched']

interface_path = '/src/components/interface/'
rec_stubs = '__stubs_rec'
normal_stubs = '__stubs'
using_stubs = 'stubs'

mm_rec_h = '__mem_mgr_rec.h'
mm_nor_h = '__mem_mgr.h'
mm_header = 'mem_mgr.h'

def query(name, default = "n"):

    valid = {"n":"normal","r":"recovery"}

    prompt = " [n/r] "
    sys.stdout.write(name + prompt)

    choice = raw_input().lower()
    if default is not None and choice == '':
       return default
    elif choice in valid.keys():
       return valid[choice]
    else:
       print "please type n/r"
       return '0'
    
def main():

	global p_rec, p_nor, p_dst

	print "n:normal, r:recovery"

 	path = os.path.dirname(os.getcwd())

	for i in range(0,len(names)):
		prefix = path + interface_path + names[i]
		os.chdir(prefix)
		
		p_rec = rec_stubs
		p_nor = normal_stubs
		p_dst = using_stubs

		if os.path.exists(p_dst):
		   os.unlink(p_dst)

		if os.path.exists(mm_header):
		   os.unlink(mm_header)

		ret = query(names[i], 'normal')

		if (ret == 'normal') or (ret == 'n'):
		   print "set to " + ret + '\n'
		   os.system("ln -s " + p_nor + " " + p_dst)
                   if (names[i] == 'mem_mgr'):
                       os.system("ln -s " + mm_nor_h + " " + mm_header)
		elif (ret == 'recovery') or (ret == 'r'):
		   print "set to " + ret + '\n'
		   os.system("ln -s " + p_rec + " " + p_dst)
                   if (names[i] == 'mem_mgr'):
                       os.system("ln -s " + mm_rec_h + " " + mm_header)
		else:
		   print "set to normal" + '\n'
		   os.system("ln -s " + p_nor + " " + p_dst)
                   if (names[i] == 'mem_mgr'):
                       os.system("ln -s " + mm_nor_h + " " + mm_header)

main()

