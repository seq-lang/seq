import json
import os
import sys
import argparse

from jupyter_client.kernelspec import KernelSpecManager
from IPython.utils.tempdir import TemporaryDirectory
from shutil import copyfile

kernel_json = {
    "argv":[sys.executable,"-m","seq_kernel", "-f", "{connection_file}"],
    "display_name":"Seq",
    "language":"seq",
}

def install_my_kernel_spec(user=True, prefix=None):
    with TemporaryDirectory() as td:
        os.chmod(td, 0o755) # Starts off as 700, not user readable
        with open(os.path.join(td, 'kernel.json'), 'w') as f:
            json.dump(kernel_json, f, sort_keys=True)

        print('Installing IPython kernel spec')
        KernelSpecManager().install_kernel_spec(td, 'seq', user=user, prefix=prefix)

def install_my_kernel_javascript():
    seq_js_file = os.path.join(os.environ['SEQ_PATH'][:-7], 'jupyter', 'seq_kernel', 'kernel.js')
    kernel_js_file = os.path.join(KernelSpecManager().get_kernel_spec('seq').resource_dir, 'kernel.js')
    os.system(f'cp {seq_js_file} {kernel_js_file}')

def _is_root():
    try:
        return os.geteuid() == 0
    except AttributeError:
        return False # assume not an admin on non-Unix platforms

def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Install KernelSpec for Seq Kernel'
    )
    prefix_locations = parser.add_mutually_exclusive_group()

    prefix_locations.add_argument(
        '--user',
        help='Install KernelSpec in user homedirectory',
        action='store_true'
    )
    prefix_locations.add_argument(
        '--sys-prefix',
        help='Install KernelSpec in sys.prefix. Useful in conda / virtualenv',
        action='store_true',
        dest='sys_prefix'
    )
    prefix_locations.add_argument(
        '--prefix',
        help='Install KernelSpec in this prefix',
        default=None
    )

    args = parser.parse_args(argv)

    user = False
    prefix = None
    if args.sys_prefix:
        prefix = sys.prefix
    elif args.prefix:
        prefix = args.prefix
    elif args.user or not _is_root():
        user = True

    install_my_kernel_spec(user=user, prefix=prefix)
    install_my_kernel_javascript()

if __name__ == '__main__':
    main()
