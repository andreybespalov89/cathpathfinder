from setuptools import setup, Extension

module = Extension(
    "datasetgradualstep_c",
    sources=["datasetgradualstep_c.c"],
)

setup(
    name="datasetgradualstep_c",
    version="0.1.0",
    description="C implementation of datasetgradualstep algorithms",
    ext_modules=[module],
    py_modules=[],
)
