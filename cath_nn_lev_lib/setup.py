from setuptools import setup, Extension

ext = Extension(
    "cath_nn_lev.cath_nn_lev",
    sources=[
        "src/cath_nn_lev.c",
        "src/fasta.c",
        "src/kmer_index.c",
        "src/topm.c",
        "src/levenshtein.c",
        "src/pipeline.c",
        "src/util.c",
    ],
    extra_compile_args=["-O3"],
)

setup(
    name="cath_nn_lev",
    version="0.1.0",
    description="Nearest neighbor search in polyfasta with k-mer prefilter and Levenshtein distance",
    packages=["cath_nn_lev"],
    ext_modules=[ext],
)
