import setuptools
from seq_kernel.kernel import __version__

with open('README.md', 'r') as fh:
    long_description = fh.read()

setuptools.setup(
    name='seq_kernel',
    version=__version__,
    description='Seq Kernel',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/seq-lang/seq',
    packages=setuptools.find_packages(),
    entry_points={'pygments.lexers': 'seq = seq_kernel.seqlex:SeqLexer'},
    classifiers=[
        'Framework :: Jupyter',
        'Intended Audience :: Science/Research',
        'Programming Language :: Python'
        'Programming Language :: Seq',
    ]
)