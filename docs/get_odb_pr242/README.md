# get_odb PR #242 — `--best_odb` / `--all_ancestor`

Draft for [sanger-tol/nf-core-modules#242](https://github.com/sanger-tol/nf-core-modules/pull/242).

Copy into the module tree:

| File here | Target in nf-core-modules |
|-----------|---------------------------|
| `get_odb.py` | `modules/sanger-tol/ncbidatasets/get_odb/resources/usr/bin/get_odb.py` |
| `main.nf` | `modules/sanger-tol/ncbidatasets/get_odb/main.nf` |

## CLI

```bash
# Default (same as genomenote): single best lineage
get_odb.py --ncbi_summary_json summary.json --lineage_tax_ids odb10 --file_out out.csv

# Explicit best
get_odb.py ... --best_odb --file_out out.csv

# All BUSCO lineages along NCBI lineage + basal
get_odb.py ... --all_ancestor --file_out out.csv
```

## Nextflow

`all_ancestral_lineages = false` → passes `--best_odb`  
`all_ancestral_lineages = true` → passes `--all_ancestor`

Tests in PR (`input[2] = false` / `true`) stay valid; snapshot hashes should be unchanged if CSV output is unchanged.
