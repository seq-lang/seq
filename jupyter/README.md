# Seq Kernel

A seq kernel for Jupyter.

## To Install

```bash
python3 -m pip install seq_kernel
python3 -m seq_kernel.install
```

## To Use

```bash
jupyter notebook
# In the notebook interface, select Seq from the 'New' menu
jupyter qtconsole --kernel seq
jupyter console --kernel seq
```

## To Deploy

1. Update the version number in `seq_kernel/__init__.py`.
1. Install flit if you don't already have it.
    ```bash
    python3 -m pip install flit
    ```
1. Test the package locally.
    ```bash
    flit install [--symlink] [--python path/to/python]
    ```
1. Run this command to upload to PyPi.
    ```bash
    flit publish
    ```
