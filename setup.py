from setuptools import setup, find_packages

setup(
    name="cquery",
    version="0.1.0",
    packages=find_packages(),
    description="Improve your query speed by using CQuery",
    long_description=open('README.md').read(),
    long_description_content_type="text/markdown",
    author="Ravshanbek Madaminov",
    author_email="ravshanbekrm06@gmail.com",
    url="https://github.com/yourusername/mypackage",
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.6',  # Minimum Python version
)
