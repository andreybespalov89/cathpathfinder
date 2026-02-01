from setuptools import setup, Extension

module = Extension(
    "datasetgradualstep_c._core",
    sources=["datasetgradualstep_c.c"],
)

setup(
    name="datasetgradualstep_c",
    version="0.1.0",
    description="C implementation of datasetgradualstep algorithms",
    ext_modules=[module],
    packages=["datasetgradualstep_c"],
    package_dir={"datasetgradualstep_c": "."},
)
