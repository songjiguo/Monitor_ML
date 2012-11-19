#!/usr/bin/python
## script helps to maintain two different version of code base
## 1. normal base
## 2. recovery base
##    able to choose the combination of above two

import os
import sys

names = ['mem_mgr','torrent', 'sched']

# interface
interface_path = '/src/components/interface/'
rec_stubs = '__stubs_rec'
normal_stubs = '__stubs'
using_stubs = 'stubs'

mm_rec_h = '__mem_mgr_rec.h'
mm_nor_h = '__mem_mgr.h'
mm_header = 'mem_mgr.h'

# component implementation
mem_man_component_path = '/src/components/implementation/mem_mgr/naive/'
mm_rec_c = '__mem_man_rec'
mm_nor_c = '__mem_man'
mem_man_c = 'mem_man.c'

sched_component_path = '/src/components/implementation/sched/'
sched_rec_c = '__cos_sched_base_rec'
sched_nor_c = '__cos_sched_base'
sched_c = 'cos_sched_base.c'

ramfs_component_path = '/src/components/implementation/torrent/ramfs/'
ramfs_rec_c = '__ramfs_rec'
ramfs_nor_c = '__ramfs'
ramfs_c = 'ramfs.c'

def query(name, default = "0"):

    valid = {"n":"normal","r":"recovery"}

    prompt = " [n/r] "
    print 
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
    path = os.path.dirname(os.getcwd())

    print
    print "setting normal version or fault tolerance version"
    print "(n:normal, r:recovery, nothing: current version)"
    for i in range(0,len(names)):
        prefix = path + interface_path + names[i]
        os.chdir(prefix)

        p_rec = rec_stubs
        p_nor = normal_stubs
        p_dst = using_stubs

        ret = query(names[i], '0')

        if (ret == '0'):
            os.system("ls -alF | grep '>' | grep 'stub'| awk '{print $8 $9 $10}'")
            continue

        if os.path.exists(p_dst):
            os.unlink(p_dst)

        if os.path.exists(mm_header):
            os.unlink(mm_header)

        # interface
        if (ret == 'normal') or (ret == 'n'):  
          os.system("ln -s " + p_nor + " " + p_dst)
        elif (ret == 'recovery') or (ret == 'r'):
            os.system("ln -s " + p_rec + " " + p_dst)
        else:
            os.system("ln -s " + p_nor + " " + p_dst)

        # component
        if (names[i] == 'mem_mgr'):
            prefix_mm = path + mem_man_component_path

            os.chdir(prefix)
            if os.path.exists(mm_header):
                os.unlink(mm_header)
            if (ret == 'normal') or (ret == 'n'):
                os.system("ln -s " + mm_nor_h + " " + mm_header)
            elif (ret == 'recovery') or (ret == 'r'):
                os.system("ln -s " + mm_rec_h + " " + mm_header)
            else:
                os.system("ln -s " + mm_nor_h + " " + mm_header)

            os.chdir(prefix_mm)
            if os.path.exists(mem_man_c):
                os.unlink(mem_man_c)
            if (ret == 'normal') or (ret == 'n'):
                os.system("ln -s " + mm_nor_c + " " + mem_man_c)
            elif (ret == 'recovery') or (ret == 'r'):
                os.system("ln -s " + mm_rec_c + " " + mem_man_c)
            else:
                os.system("ln -s " + mm_nor_c + " " + mem_man_c)


        if (names[i] == 'sched'):
            prefix_sched = path + sched_component_path

            os.chdir(prefix_sched)

            if os.path.exists(sched_c):
                os.unlink(sched_c)
            if (ret == 'normal') or (ret == 'n'):
                os.system("ln -s " + sched_nor_c + " " + sched_c)
            elif (ret == 'recovery') or (ret == 'r'):
                os.system("ln -s " + sched_rec_c + " " + sched_c)
            else:
                os.system("ln -s " + sched_nor_c + " " + sched_c)

        if (names[i] == 'torrent'):
            prefix_ramfs = path + ramfs_component_path

            os.chdir(prefix_ramfs)

            if os.path.exists(ramfs_c):
                os.unlink(ramfs_c)
            if (ret == 'normal') or (ret == 'n'):
                os.system("ln -s " + ramfs_nor_c + " " + ramfs_c)
            elif (ret == 'recovery') or (ret == 'r'):
                os.system("ln -s " + ramfs_rec_c + " " + ramfs_c)
            else:
                os.system("ln -s " + ramfs_nor_c + " " + ramfs_c)

main()

