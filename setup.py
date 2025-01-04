from setuptools import setup, Extension

mysql_include = r"C:\Program Files\MySQL\MySQL Server 8.0\include"  # Adjust the path as needed
mysql_lib = r"C:\Program Files\MySQL\MySQL Server 8.0\lib"

module = Extension('cquery', sources=['cquery/crud.c'],
                    include_dirs=[mysql_include],
                    library_dirs=[mysql_lib],
                    libraries=['mysqlclient', 'ws2_32'],
                    extra_link_args=['-lmysqlclient'])


setup(
    name='cquery',
    version='1.0',
    description='A CPython extension for CRUD operations with MySQL',
    ext_modules=[module],
)
