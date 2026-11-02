#!/bin/bash
# Usage: bash scripts/compare_pb_files.sh [--files|--directories] DIRECTORY1 DIRECTORY2
# --files (default): compare .pb files directly inside the two directories.
# --directories: compare matching subdirectories inside the two directories.

script_dir=$(cd "$(dirname "$0")" && pwd)
proto_dir="$script_dir/../proto"

# Decode plans, remove plan IDs, and normalize the one intermediate table name per plan.
normalize_pb() {
    local file=$1
    local output=$2

    protoc --proto_path="$proto_dir" --decode=QueryPlan QueryPlan.proto \
        < "$file" > "$output.decoded" || return 1
    sed -E '/^[[:space:]]*plan[Ii]d: /d; s/(tabName: ")i_[0-9a-f]{32}"/\1intermediate"/' \
        "$output.decoded" > "$output"
}

compare_directories() {
    local dir1=$1
    local dir2=$2
    local lists=$(mktemp -d)
    local name
    local prefix=$3

    # List filenames first, then compare only shared names.
    find "$dir1" -maxdepth 1 -type f -name '*.pb' -exec basename {} \; | sort > "$lists/first"
    find "$dir2" -maxdepth 1 -type f -name '*.pb' -exec basename {} \; | sort > "$lists/second"
    comm -12 "$lists/first" "$lists/second" > "$lists/common"

    if [ ! -s "$lists/first" ]; then
        echo "  $dir1" >> "$report/empty"
    fi
    if [ ! -s "$lists/second" ]; then
        echo "  $dir2" >> "$report/empty"
    fi

    while IFS= read -r name; do
        if normalize_pb "$dir1/$name" "$lists/a" && normalize_pb "$dir2/$name" "$lists/b"; then
            if cmp -s "$lists/a" "$lists/b"; then
                echo "  $prefix$name" >> "$report/identical"
            else
                echo "  $prefix$name" >> "$report/different"
            fi
        else
            echo "  $prefix$name" >> "$report/errors"
        fi
    done < "$lists/common"

    comm -23 "$lists/first" "$lists/second" | while IFS= read -r name; do
        echo "  $prefix$name" >> "$report/not_in_b"
    done

    comm -13 "$lists/first" "$lists/second" | while IFS= read -r name; do
        echo "  $prefix$name" >> "$report/not_in_a"
    done

    rm -r "$lists"
}

mode=--files
case "$1" in
    --files|--directories)
        mode=$1
        shift
        ;;
esac

dir1=$1
dir2=$2
first_dirs=()
second_dirs=()
prefixes=()
report=$(mktemp -d)
for group in identical different not_in_a not_in_b empty errors; do
    touch "$report/$group"
done

if [ "$mode" = --directories ]; then
    lists=$(mktemp -d)
    find "$dir1" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort > "$lists/first"
    find "$dir2" -mindepth 1 -maxdepth 1 -type d -exec basename {} \; | sort > "$lists/second"
    comm -12 "$lists/first" "$lists/second" > "$lists/common"

    if [ ! -s "$lists/first" ]; then
        echo "  $dir1 (no subdirectories)" >> "$report/empty"
    fi
    if [ ! -s "$lists/second" ]; then
        echo "  $dir2 (no subdirectories)" >> "$report/empty"
    fi

    while IFS= read -r name; do
        first_dirs+=("$dir1/$name")
        second_dirs+=("$dir2/$name")
        prefixes+=("$name/")
    done < "$lists/common"

    comm -23 "$lists/first" "$lists/second" | while IFS= read -r name; do
        echo "  $name/ (directory)" >> "$report/not_in_b"
    done
    comm -13 "$lists/first" "$lists/second" | while IFS= read -r name; do
        echo "  $name/ (directory)" >> "$report/not_in_a"
    done
    rm -r "$lists"
else
    first_dirs+=("$dir1")
    second_dirs+=("$dir2")
    prefixes+=("")
fi

for index in "${!first_dirs[@]}"; do
    compare_directories "${first_dirs[$index]}" "${second_dirs[$index]}" "${prefixes[$index]}"
done

echo "Comparison summary"
echo "A: $dir1"
echo "B: $dir2"
labels=("Identical (after normalization)" "Mismatching" "Not in A" "Not in B" "Empty directories" "Could not normalize")
groups=(identical different not_in_a not_in_b empty errors)
for index in "${!groups[@]}"; do
    echo ""
    echo "${labels[$index]}:"
    if [ -s "$report/${groups[$index]}" ]; then
        cat "$report/${groups[$index]}"
    else
        echo "  None"
    fi
done
rm -r "$report"
