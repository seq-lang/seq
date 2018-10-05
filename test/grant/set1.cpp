// 786
// benchmarks:
// - reverse complementing 10g FASTA
// - printing all 16-mers from 10g FASTA
// - count number of CpG islands (e.g. all CG-only subseqs)

#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

char revcomp(char c) {
   return (c == 'T' ? 'A' :
          (c == 'G' ? 'C' :
          (c == 'C' ? 'G' :
          (c == 'A' ? 'T' : c))));
}

string rc_copy(string s) {
   string x = s;
   for (int i = 0; i < x.size(); i++)
      x[i] = revcomp(s[s.size() - i - 1]);
   return x;
}

void exp1(ifstream &fin) { 
   string s;
   long long total = 0;
   while (getline(fin, s)) {
      auto y = rc_copy(s);
      total += y.size();
   }
   cout << total << endl;
}

void exp2(ifstream &fin) { 
   string s;
   long long total = 0, total2 = 0;
   while (getline(fin, s)) {
      int i = 0;
      int k = 16;
      int step = 1;
      while (i + k < s.size()) {
         total += s.substr(i, k).size();
         total2++;
         i += step;
      }
   }
   cout << total << ' ' << total2 << endl;
}

bool is_cpg(char s) {
   return (s == 'C' || s == 'G');
}

long long cpg_count(string s) {
   long long count = 0;
   int i = 0;
   while (i < s.size()) {
      if (is_cpg(s[i])) {
         int j = i + 1;
         while (j < s.size() && is_cpg(s[j])) {
            j++;
         }
         count++;
         i = j + 1;
      } else i++;
   }
   return count;
}

void exp3(ifstream &fin) { 
   string s;
   long long res = 0;
   while (getline(fin, s)) {
      res += cpg_count(s);
   }
   cout << res << endl;
}

int main(int argc, char **argv) {
   ios_base::sync_with_stdio(false);
	cin.tie(nullptr);

   ifstream fin(argv[1]);
   switch (argv[2][0]) {
      case '1': exp1(fin); break;
      case '2': exp2(fin); break;
      case '3': exp3(fin); break;
      default: cerr << "whoops" << endl;
   }
   return 0;
}
