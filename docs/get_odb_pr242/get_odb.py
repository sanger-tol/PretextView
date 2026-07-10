#!/usr/bin/env python3
"""
get_odb.py for nf-core-modules PR #242
Originally by Matthieu Muffato; modified for module packaging.
CLI: mutually exclusive --best_odb (default) or --all_ancestor
"""

import argparse
import json
import os
import sys

import requests

NCBI_TAXONOMY_API = "https://api.ncbi.nlm.nih.gov/datasets/v2/taxonomy/taxon/%s"


def parse_args(args=None):
    description = "Get ODB database value using NCBI API and BUSCO configuration file"
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument(
        "--ncbi_summary_json",
        required=True,
        help="NCBI entry for this assembly (JSON).",
    )
    parser.add_argument(
        "--lineage_tax_ids",
        required=True,
        help="ODB version key: odb10 or odb12 (mapping TSV bundled next to this script).",
    )
    parser.add_argument("--file_out", required=True, help="Output CSV file.")
    output_mode = parser.add_mutually_exclusive_group()
    output_mode.add_argument(
        "--best_odb",
        action="store_true",
        help="Output only the best-matching BUSCO lineage (default; matches genomenote).",
    )
    output_mode.add_argument(
        "--all_ancestor",
        action="store_true",
        help="Output all ancestor BUSCO lineages on the taxonomic path, plus basal lineages.",
    )
    parser.add_argument(
        "--basal_lineages",
        help="Lineages considered ancestral for ODB selection when using --all_ancestor.",
        nargs="+",
        default=["bacteria", "archaea", "eukaryota"],
    )
    parser.add_argument("--version", action="version", version="%(prog)s 1.3")
    return parser.parse_args(args)


def make_dir(path):
    if len(path) > 0:
        os.makedirs(path, exist_ok=True)


def get_odb(ncbi_summary, lineage_tax_ids, file_out, all_ancestor, basal_lineages):
    mapping: dict[str, dict[str, str]] = {
        "odb10": {"file": "odb10_mapping.tsv", "version": "_odb10"},
        "odb12": {"file": "odb12_mapping.tsv", "version": "_odb12"},
    }

    current_working_dir = os.path.dirname(os.path.realpath(__file__))
    if lineage_tax_ids not in mapping:
        sys.exit("Not a recognised ODB")

    mapping_file = f"{current_working_dir}/{mapping[lineage_tax_ids]['file']}"
    with open(mapping_file) as file_in:
        lineage_tax_ids_dict = {}
        for line in file_in:
            arr = line.split()
            lineage_tax_ids_dict[int(arr[0])] = arr[1]

    with open(ncbi_summary) as file_in:
        data = json.load(file_in)
    tax_id = data["reports"][0]["organism"]["tax_id"]

    response = requests.get(NCBI_TAXONOMY_API % tax_id).json()
    ancestor_taxon_ids = response["taxonomy_nodes"][0]["taxonomy"]["lineage"]

    odb_arr = [
        lineage_tax_ids_dict[taxon_id]
        for taxon_id in ancestor_taxon_ids
        if taxon_id in lineage_tax_ids_dict
    ]

    odb_version: str = mapping[lineage_tax_ids]["version"]

    if not odb_arr:
        sys.exit("No BUSCO lineage found in NCBI lineage for this assembly")

    if all_ancestor:
        odb_val: list[str] = [lineage + odb_version for lineage in odb_arr]
        odb_val = odb_val + [
            lineage + odb_version
            for lineage in basal_lineages
            if lineage not in odb_arr
        ]
    else:
        # default / --best_odb: single closest lineage (genomenote behaviour)
        odb_val = [odb_arr[-1] + odb_version]

    out_dir = os.path.dirname(file_out)
    make_dir(out_dir)

    with open(file_out, "w") as fout:
        for lineage in odb_val:
            print("busco_lineage", lineage, sep=",", file=fout)


def main(args=None):
    args = parse_args(args)
    get_odb(
        args.ncbi_summary_json,
        args.lineage_tax_ids,
        args.file_out,
        args.all_ancestor,
        args.basal_lineages,
    )


if __name__ == "__main__":
    main()
