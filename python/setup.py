from distutils.core import setup, Extension
import sysconfig

setup(
    name = 'histograms',
    author = 'Ivan Pogrebnyak',
    url='https://github.com/ivankp/histograms',
    ext_modules = [
    Extension('histograms',
        sources = ['histograms.cc'],
        libraries=[],
        include_dirs = ['../include'],
        language = 'c++17',
        extra_compile_args = sysconfig.get_config_var('CFLAGS').split() +
            ['-std=c++17','-Wall','-O3'],
        extra_link_args = ['-lstdc++']
    )
])
