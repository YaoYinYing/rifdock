#########################################################################
# File Name: commands.sh
# Author: longxing
# mail: longxing@uw.edu
# Created Time: Wed 02 Sep 2020 03:51:47 PM PDT
#########################################################################
#!/bin/bash

# 1) run rifgen
/path/to/rifgen @rifdock_input/rifgen.flag

# 2) run rifdock
/path/to/rif_dock_test -scaffolds scaffolds/*.pdb @rifdock_input/rifdock.flag

# 3) run fastdesign to optimize the interface
/path/to/rosetta_scripts.hdf5.linuxgccrelease -s rifdock_out/HHH_b1_00224_000000001.pdb.gz -parser:protocol COVID19_design_fastdesign.xml -corrections:beta_nov16
