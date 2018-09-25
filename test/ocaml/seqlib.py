# Seq library 

extend int:
  def iter(self):
    i = 0
    while i < self:
      yield i
      i += 1


extend file:
  def iter_helper(self, fn):
    while True:
      n = self.read()
      if n == 0:
        break
      for i in n.iter():
        yield fn(i)
    self.close()

  def iter(self):
    for i in self.iter_helper(self.get):
      yield i

  def iter_multi(self):
    for i in self.iter_helper(self.get_multi):
      yield i

      

