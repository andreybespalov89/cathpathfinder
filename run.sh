#!/bin/bash
python3 run_trajectory_layers.py --fasta validation_outputs/new_sequences.fasta --out-dir trajectory_levels --gpus 0,1 --num-samples 3 --rmsd-threshold 2.0 --skip-existing
