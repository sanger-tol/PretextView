process NCBIDATASETS_GET_ODB {
    tag "${meta.id}"
    label 'process_single'

    conda "${moduleDir}/environment.yml"
    container "${workflow.containerEngine in ['singularity', 'apptainer'] && !task.ext.singularity_pull_docker_container
        ? 'https://community-cr-prod.seqera.io/docker/registry/v2/blobs/sha256/34/34a3fbdf8feee942542033e0c6961cd1efb0f4b2d4ec580982c653a74a73949d/data'
        : 'community.wave.seqera.io/library/requests:2.34.2--26550d4c3ba1cb75' }"

    input:
    tuple val(meta), path(ncbi_summary)
    val lineage_tax_ids
    val all_ancestral_lineages

    output:
    tuple val(meta), path("*.busco_odb.csv"), emit: csv
    tuple val("${task.process}"), val('python'), eval('python --version | sed "s/Python //"'), emit: versions_python, topic: versions
    tuple val("${task.process}"), val('get_odb.py'), eval('get_odb.py --version | cut -d" " -f2'), emit: versions_get_odb, topic: versions

    when:
    task.ext.when == null || task.ext.when

    script:
    def args = task.ext.args ?: ''
    def prefix = task.ext.prefix ?: "${meta.id}"
    def odb_mode = all_ancestral_lineages ? '--all_ancestor' : '--best_odb'
    """
    get_odb.py \\
        --ncbi_summary_json ${ncbi_summary} \\
        --lineage_tax_ids ${lineage_tax_ids} \\
        ${odb_mode} \\
        ${args} \\
        --file_out ${prefix}.busco_odb.csv
    """

    stub:
    def prefix = task.ext.prefix ?: "${meta.id}"
    """
    touch ${prefix}.busco_odb.csv
    """
}
