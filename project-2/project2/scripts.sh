#!/bin/bash

n_proc_values=(1 2 4 8)
n_thread_values=(1 2 4 6 12)

output_dir="generated_scripts"
mkdir -p "$output_dir"

for n_proc in "${n_proc_values[@]}"; do
  for n_thread in "${n_thread_values[@]}"; do
    script_name="$output_dir/combined_${n_proc}_${n_thread}.sbatch"
    cat > "$script_name" <<EOL
#!/bin/bash

#SBATCH -J cs6210-proj2-combinedhello
#SBATCH -N $n_proc --ntasks-per-node=1 --cpus-per-task=$n_thread
#SBATCH --mem-per-cpu=1G
#SBATCH -t 20
#SBATCH -q coc-ice
#SBATCH -o combined_${n_proc}_${n_thread}.out
#SBATCH --chdir=/home/hice1/tjang31/repo/cs6210/project-2/project2/combined

echo "Started on \`/bin/hostname\`"

module load gcc/12.3.0 mvapich2/2.3.7-1
export OMP_NUM_THREADS=$n_thread

srun -n $n_proc ./combined $n_thread
EOL
    chmod +x "$script_name"
    echo "Generated: $script_name"
  done
done