# machine info
nb_physical_cores=$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
# used by both
nb_solvers=$nb_physical_cores # the remaning one for sharer

# painless only
verbose=1
timeout=500
solver="kc"
lstrat=-1
gstrat=2
shr_sleep=500000
sharing_per_sec=2
gshr_lit=$(($nb_physical_cores * 400 / $sharing_per_sec))
flags="-simple -mallob -gshr-sleep=10000 -gshr-lit=${gshr_lit} -importDB-cap=$(($gshr_lit * 10)) "  #"-simple -mallob -gshr-sleep=10000 -importDB=m" #"-gshr-lit=0 -one-sharer -gshr-sleep=10000" # -dup -one-sharer -dist -simp -no-sbva-shuffle -sbva-count=12 -ls-after-sbva=2 -sbva-timeout=1000


# mpi only
nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
# nb_procs_per_node=3
hostfile=$HOSTFILE
nb_nodes=$(ls $hostfile | wc)
