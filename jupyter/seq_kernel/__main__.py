from ipykernel.kernelapp import IPKernelApp
from .kernel import SeqKernel
IPKernelApp.launch_instance(kernel_class=SeqKernel)