# Seq library 

# extend int:
#   def iter(self):
#     i = 0
#     while i < self:
#       yield i
#       i += 1


# extend Source:
#   def iter_helper(self, fn):
#     while true:
#       n = self.read()
#       if n == 0:
#         break
#       for i in n.iter():
#         yield fn(i)
#     self.close()

#   def iter(self):
#     iter_helper(self, self.get)
#   def iter_multi(self):
#     iter_helper(self, self.get_multi)

      

