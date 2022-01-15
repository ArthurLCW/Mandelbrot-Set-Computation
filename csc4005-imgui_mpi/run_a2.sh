#!/bin/bash
#SBATCH --account=csc4005
#SBATCH --partition=debug
#SBATCH --qos=normal
#SBATCH --ntasks=100
#SBATCH --nodes=4
#SBATCH --output thr_800_100_core_mc.out
#SBATCH --time=2



echo "mainmode: " && /bin/hostname
xvfb-run -a mpirun /pvfsmnt/118010134/a2/csc4005-imgui_mpi/build/csc4005_imgui 100
