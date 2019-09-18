# Exec

class SeqWrapper:
   def __init__(self):
      import ctypes
      import ctypes.util

      lib = ctypes.util.find_library("libseqjit")
      self._lib = ctypes.CDLL(lib)
      self._init_fn = self._lib.caml_jit_init
      self._init_fn.restype = ctypes.c_void_p
      self._exec_fn = self._lib.caml_jit_exec
      self._exec_fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
      self.handle = self._init_fn()

   def exec(self, code):
      self._exec_fn(self.handle, code.encode('utf-8'))


s = SeqWrapper()

s.exec("""
print 'hello'
x = 1
print x
#y = 2
#print x, y #, x + y
""")

s.exec("""
x += 1
print x
""")