# comment

def f(x, y):
    return x + y

def f(x of int, y of int) of int:
    return x + y

def f(x): # comment
    yield x |> collect(33)

x = 5 # var, ok
y = s'22'

for i in range(5):
    if i >= 1:
        print "hello"
        break
    elif i < 1 or y == 4 and not z >= 4: # TODO
        pass
    else:
        print "whoops"
        continue

while j > 0:
    print j
    j = j - 1 

# match x:
#     case "AA..AA" as y:
#         print y
#     case 2: 
#         print 2
#     default:
#         print 

y = read(x, y) |> split(32)

y = source(args[0]) |> substr(0+1, 5) |> branch([
    f_print,
    revcomp |> f_print
])

str = "ACBD\nCGTA"

print a[1]
print a[-1]
a = {1: "1", 2: "2"}

y = True
y = y or False 

b = Seq("ACGT")
# print a + (b as string)

# y = `py __version__` 
# alternative: py`y = __version__

# Stage 2

if x > 3: print x # TODO
while x < 3: x += 1 # TODO
for i in range(4): print i # TODO

j -= 1
j += 1


# import `C
# import `R

# extern `C.pow(x of double) of double

# num = `C.pow(3.0) 
# # auto pass back
# py`num = `Python.math.pow(3.0)

# # auto translation
# # int, bool, string...
# # type(a, b, c) -> struct of ...
# # TODO: void*


# case r"AA..AA" as y
# a = 'hello' # TODO single quoted strings
# b = s'ACGT' # TODO
# print a + b as string # TODO (as operator; precedence)

# a, b, c = [1, 2, 3], [1], []
# print a[1:3:-2], a[::-1], a[1::-1], a[:1:-1] # TODO

# type Person(name of string, age of int) # TODO
# p = Person('hi', 32) # TODO 
# print p[0], p.age # TODO

# def print_pe(p of Person, y):
#     return p.name + y.name

# wn = (1, 2, 'three')
# wn[0], wn.0
# wn[1], wn.1
# print wn[2], wn.2
# print wn[1:3] # list
# ww = (1,) + (2, )
# wx = (1)

# # Final stages
# import fn # TODO
# # def error(x):
# #     for i in [1, 2, 3]:
# #         pass
# #             y = 'cool' # TODO indent levels
# #         yield i
# # x! = 5 
