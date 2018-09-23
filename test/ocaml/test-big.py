# Testbed !

def range(a: int, b):
   i = a
   while i < b:
      yield i
      i += 1

def main(args: array[str]):   
   def split(s: seq, k, step: int): # TODO: step fails w/o type annotation
      i = 0
      while i + k < s.len:
         yield s[i:i + k]
         i += step
   
   def enum(s: array[seq]): # TODO: fails w/o type annotation
      i = 0
      for j in range(0, s.len):
         yield (i, s[j])
         i += 1
   
   def f(s: seq): # TODO: fails w/o type annotation
      v = array[seq](s.len / 32)
      i = 0
      for t in split(s, 32, 32):
         v[i] = t
         i += 1
      return v
   
   farr = array[typeof(f)](1)
   farr[0] = f

   def ident(s):
      return s

   a = array[seq](2)
   a[0] = s'ACGT'
   a[1] = s'NANANANANANANANANANANANANANANANA...BATMAN!'
   for i in enum(a):
      print 'internal', i[0], i[1] 
   y = f(a[1])
   print y.len
   for i in range(0, y.len):
      print 'infernal', y[i]
   
   # def process_input(input):
   #    v = farray[0](input)
   #    for r in enum(v):
   #       print r[0], r[1]
   #    print input
   #    print ident(input)

   #    arr = array[type(int, seq)](10)

   #    match input:
   #       #case 'A...' as q if q.len > 150: 
   #       #   print 'case A with if'
   #       case 'C...': print 'match C'
   #       case 'G...': print 'match G'
   #       case '_...': print 'match _'
   #       default: print 'default'
      
   #    for v1 in split(input, 64, 64):
   #       print v1
   #       for v2 in split(v1, 32, 32):
   #          for v3 in split(v2, 16, 16):
   #             print v3
   #          print v2
   #       print v1
   #    print ident(input)
   #    print (v[1], ((v)[2]), (v[1], 4.2))[2][0]
   # source(args) |> process_input

main(array[str](0))

###############################################################################
################################### Stage 2 ###################################
###############################################################################

def hello():
  return "hello world"

def hello2():
  print "hello again"

print hello()
hello2()
print 42 + 2.2
print -13
x = 4.2
print x
x = .42
print -x if x < 1 else +x
b = (True, not True)
print b[0]
b = (b[0], not not False) # TODO: bad shadow warning
print b[1]

# def fib(n: int) -> int:
#    type F(f: callable[int, int], x: int)
   
#    def apply(f: callable[int, int], x: int): 
#       return f(x)
#    def rec(pair: F):
#       return apply(pair.f, pair.x - 1) + apply(pair.f, pair.x - 2)
#    return n if n <= 1 else rec(F(fib, n))
# def fib(n: int) -> int:
   # return n if n <= 1 else fib(n-1)+fib(n-2)
# print fib(10)

for i in range(1 if b[0] else 2, 5 + 5):
   if i == 3: continue
   elif i >= 7:
      break
   else: 
      for z in range(i, i + 1): print z

###############################################################################
################################### Stage 3 ###################################
###############################################################################

type Person(name: str, age: int)

def whois(person: Person):
  print person.name
  print person.age

def birthday(person: Person):
  return Person(person.name, person.age + 1)

joe = Person("Joe", 42) # TODO: named args
whois(joe)
joe = birthday(joe)
whois(joe)

# # global ints = Int[100]
# # for i in ints.len.iter():
# #   ints[i] = i+1
# # global s = 0
# # for z in ints.len.iter(): s = s + ints[z]
# # print s

# class Pair[`t](a: `t, b: `t):
#    def sum(self, c):
#       return self.a + self.b + c
#    def dump(self):
#       print 'Pair:', self.a, self.b
#    def swap(self):
#       tmp = self.a
#       self.a = self.b
#       self.b = tmp
   
#    def iter(self):
#       yield self.a
#       yield self.b


# p = Pair[int](42, 19)
# print p.sum(100.1)  # 42 + 19 + 100 = 161
# print p.sum(100)
# p.dump()  # 42,19
# p.swap()
# for a in p.iter(): print a  # 19,42

# q = Pair(4.2, 1.9)
# print q.sum(10.0)
# for a in q.iter(): print a
# q.swap()
# pair[float].dump(q)






