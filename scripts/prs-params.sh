# machine info
nb_physical_cores=$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
# used by both
nb_solvers=$nb_physical_cores # the remaning one for sharer

# painless only
verbose=1
timeout=500
solver="IMl"
lstrat=1
gstrat=3
shr_sleep=500000
flags="-gshr-lit=0 -one-sharer -gshr-sleep=0"


# mpi only
nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
# nb_procs_per_node=3
hostfile=$HOSTFILE
nb_nodes=$(ls $hostfile | wc)
