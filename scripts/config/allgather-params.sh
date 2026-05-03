# machine info
nb_physical_cores=$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
# used by both
nb_solvers=$nb_physical_cores # the remaning one for sharer

# painless only
verbose=1
timeout=500
solver="kc"
lstrat=1
gstrat=1
shr_sleep=200000
gshr_lit=$(($nb_physical_cores * 1500))
flags="-simple -gshr-sleep=300000 -gshr-lit=${gshr_lit}"


# mpi only
nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
# nb_procs_per_node=3
hostfile=$HOSTFILE
nb_nodes=$(ls $hostfile | wc)
