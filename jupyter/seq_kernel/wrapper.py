# Exec

class SeqWrapper:
   def __init__(self):
      import ctypes
      import ctypes.util

      lib = ctypes.util.find_library("seqjit")
      self._lib = ctypes.CDLL(lib, ctypes.RTLD_GLOBAL)

      self._init_fn = self._lib.seq_jit_init
      self._init_fn.restype = ctypes.c_void_p

      self._exec_fn = self._lib.seq_jit_exec
      self._exec_fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]

      self._inspect_fn = self._lib.seq_jit_inspect
      self._inspect_fn.argtypes = [
         ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int
      ]
      self._inspect_fn.restype = ctypes.c_char_p

      self._document_fn = self._lib.seq_jit_document
      self._document_fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
      self._document_fn.restype = ctypes.c_char_p

      self._complete_fn = self._lib.seq_jit_complete
      self._complete_fn.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
      self._complete_fn.restype = ctypes.c_char_p

      self.handle = self._init_fn()

   def exec(self, code):
      self._exec_fn(self.handle, code.encode('utf-8'))

   def inspect(self, cell, line, column):
      file = f'<jit_{cell}>'
      return self._inspect_fn(self.handle, file.encode('utf-8'), line, column).decode('ascii')

   def document(self, idn):
      return self._document_fn(self.handle, idn.encode('utf-8')).decode('ascii')

   def complete(self, prefix):
      l = self._complete_fn(self.handle, prefix.encode('utf-8')).decode('ascii')
      return l.split('\b')
