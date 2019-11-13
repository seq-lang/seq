def multiply(a,b):
    return a*b

def print_args(a,b,c,d,e):
    t = (a,b,c,d,e)
    print 'py', t
    return ({'a': 3.14, 'b': 2.123}, (222, 3.14))

def throw_exc():
    raise IOError('foo')
    return 0
