#!/bin/bash
# Usage: bash scripts/gen_pb_plans_all_opts.sh FOLDER_NAME


script_dir=$(cd "$(dirname "$0")" && pwd)
output_dir="$script_dir/../generated/pb_plans"
run_dir="$output_dir/$1"


# Enumerate all (mat_type, gco, merge_sort) tuples first.
tuples=(
    "std,true,true"
    "std,true,false"
    "std,false,true"
    "std,false,false"
    "lateMat,true,true"
    "lateMat,true,false"
    "lateMat,false,true"
    "lateMat,false,false"
    "lateMatHybrid,true,true"
    "lateMatHybrid,true,false"
    "lateMatHybrid,false,true"
    "lateMatHybrid,false,false"
)

for tuple in "${tuples[@]}"; do
    IFS=, read -r mat_type gco merge_sort <<< "$tuple"

    gco_flag=--no-gco
    if [ "$gco" = true ]; then
        gco_flag=--gco
    fi
    merge_sort_flag=--no-merge-sort
    if [ "$merge_sort" = true ]; then
        merge_sort_flag=--merge-sort
    fi

    echo "Generating plans for $tuple"
    bash "$script_dir/generate_pb_plans.sh" --sql --mat-type "$mat_type" \
        "$gco_flag" "$merge_sort_flag" || exit 1

    # The generator writes directly into pb_plans; move its files per tuple.
    mkdir -p "$run_dir/$tuple"
    mv "$output_dir"/*.pb "$run_dir/$tuple/"
done
