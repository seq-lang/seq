# 786
# benchmarks:
# - reverse complementing 10g FASTA
# - printing all 16-mers from 10g FASTA
# - count number of CpG islands (e.g. all CG-only subseqs)

import sys

def split(s, k, step):
   i = 0
   while i + k < len(s):
      yield s[i:i + k]
      i += step

def is_cpg(s):
   return s == 'C' or s == 'G'

def cpg(s):
   i = 0
   while i < len(s):
      if is_cpg(s[i]):
         j = i + 1
         while j < len(s) and is_cpg(s[j]):
            j += 1
         yield s[i:j]
         i = j + 1
      else: i += 1

def cpg_count(s):
   count = 0
   for _ in cpg(s):
      count += 1
   return count

def revcomp(c):
   return ('A' if c == 'T' else \
          ('C' if c == 'G' else \
          ('G' if c == 'C' else \
          ('T' if c == 'A' else c))))

def rc_copy(s):
   cp = ''.join(revcomp(s[len(s) - i - 1]) for i in range(len(s)))
   return cp

with open(sys.argv[1], 'r') as f:
   if sys.argv[2] == '1':
      for l in f:
         print(rc_copy(l.strip()))
   elif sys.argv[2] == '2':
      for l in f:
         for s in split(l.strip(), 16, 1):
            print(s)
   elif sys.argv[2] == '3':
      cnt = 0
      for l in f:
         cnt += cpg_count(l.strip())
      print(cnt)
   else:
      print('whoops')
