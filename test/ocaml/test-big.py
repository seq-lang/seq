# Testbed !

import seqlib

ii = 5
for i in ii.iter():
   print 'woohoo', i

def range(a, b):
   i = a
   while i < b:
      yield i
      i += 1

def main(args: array[str]):   
   def split(s, k, step):
      i = 0
      while i + k < s.len:
         yield s[i:i + k]
         i += step
   
   def enum(s): 
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
   
   def process_input(input):
      v = f(input) #  farr[0](input) -- currently not inheriting vars from function
      for r in enum(v):
         print r[0], r[1]
      print input
      print ident(input)
      print 'len len len', v.len

      # arr = array[type(int, seq)](10)


      print 'eee', v[0]
      # TODO Fails
      # match v[0]:
      #    #case 'A...' as q if q.len > 150: 
      #    #   print 'case A with if'
      #    case 'CCTGCATCACGACGACCGCCGCCACCGTCAGC': 
      #       print 'caught sth'
      #    # case 5: print 'no way' # LLVM exception
      #    default: print 'default'
      
      for v1 in split(input, 64, 64):
         print v1
         for v2 in split(v1, 32, 32):
            for v3 in split(v2, 16, 16):
               print v3
            print v2
         print v1
      print ident(input)
      # print v[1]
      #a = (v[1], v[2], (v[1], 4.2)) CRASHES
      #print a[2][0]
   # file(args[0]).iter() |> process_input(...) # TODO CRASHES
   for i in file(args[0]).iter():
   	process_input(i)


args = array[str](1)
args[0] = '../test/data/multiple/seqs.fastq'
main(args)

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

def fib(n: int) -> int:
   type F(f: callable[int, int], x: int)
   def apply(f: callable[int, int], x: int): 
      return f(x)
   def rec(pair: F):
      return apply(pair.f, pair.x - 1) + apply(pair.f, pair.x - 2)
   return n if n <= 1 else rec(F(fib, n))
def fib2(n) -> int:
   return n if n <= 1 else fib2(n-1)+fib2(n-2)
print 'fibby', fib(10), fib2(10)

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

class Pair[`t](a: `t, b: `t):
   def sum(self, c):
      return self.a + self.b + c
   def dump(self):
      print 'Pair:', self.a, self.b
   def swap(self):
      tmp = self.a
      self.a = self.b
      self.b = tmp
   
   def iter(self):
      yield self.a
      yield self.b


p = Pair[int](42, 19)
print p.sum(100.1)  # 42 + 19 + 100 = 161
print p.sum(100)
p.dump()  # 42,19
p.swap()
for a in p.iter(): print a  # 19,42

q = Pair(4.2, 1.9)
print q.sum(10.0)
for a in q.iter(): print a
q.swap()
# Pair[float].dump(q) # TODO requires explicit type on dump --- expected function input type '``self', but got 'Pair[Float]'

# def print_coord(t: Type(Int, Int, Str)):
# 	match t:
# 		case (x, y, _) if x == y && x < 5: print "EQUAL"
# 		case (x @ (0 | 1), y @ 0...1, a @ "a") { print a ; print x ; print y }
# 		case (x, 0, "b"): print x
# 		case (0, y, "b"): print y
# 		case (x, y, "b") { print x ; print y }
# 		case (x, y, z) {}

# print_coord((0, 1, "a"))
# print "---"
# print_coord((13, 0, "b"))
# print "---"
# print_coord((11, 22, "b"))


def add[`T](a: `T, b: `T, c: `T):
  return a + b + c

add_one = add(1)
add_two = add[int](3, ..., -1)
print add[int](1, 2, 3)
print add_one(5, 0)
print 42 |> add_two

# Pair[Str].main[Float](args)
# Pair[Seq].main[Int](args)







