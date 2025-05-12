#!/bin/bash

# Run all generated scripts
for script in generated_scripts/*.sbatch; do
  echo "Submitting: $script"
  sbatch "$script" &
done