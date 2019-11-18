class SeqWrapper:
   def __init__(self):
      import ctypes
      import ctypes.util

      lib = ctypes.util.find_library("seqjit")
      self._lib = ctypes.CDLL(lib, ctypes.RTLD_GLOBAL)
      self._init_fn = self._lib.caml_jit_init
      self._init_fn.restype = ctypes.c_void_p
      self._exec_fn = self._lib.caml_jit_exec
      self._exec_fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
      self.handle = self._init_fn()

   def exec(self, code):
      self._exec_fn(self.handle, code.encode('utf-8'))
