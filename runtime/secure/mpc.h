#ifndef __MPC_H_
#define __MPC_H_

#include "connect.h"
#include "param.h"
#include "util.h"
#include <map>
#include <vector>
#include <stack>
#include <NTL/mat_ZZ_p.h>
#include <NTL/mat_ZZ.h>
#include <sstream>
#include <fstream>
#include <chrono>
#include <NTL/BasicThreadPool.h>

using namespace NTL;
using namespace std;

class MPCEnv {
public:
  MPCEnv() {};
  bool Initialize(int pid, vector< pair<int, int> > &pairs);
  bool SetupChannels(vector< pair<int, int> > &pairs);
  bool SetupPRGs(vector< pair<int, int> > &pairs);
  void CleanUp();

  /* Logging */
  void ProfilerResetTimer();
  void ProfilerPushState(string desc);
  void ProfilerPopState(bool write);
  void ProfilerWriteToFile();

  /* Logistic regression */
  void ParallelLogisticRegression(Vec<ZZ_p>& b0, Mat<ZZ_p>& bv, Vec<ZZ_p>& bx,
                                Mat<ZZ_p>& xr, Mat<ZZ_p>& xm,
                                Mat<ZZ_p>& vr, Mat<ZZ_p>& vm,
                                Vec<ZZ_p>& yr, Vec<ZZ_p>& ym,
                                int max_iter);
  void NegLogSigmoid(Vec<ZZ_p>& b, Vec<ZZ_p>& b_grad, Vec<ZZ_p>& a);

  /* Linear algebra operations */
  void Householder(Vec<ZZ_p>& v, Vec<ZZ_p>& x);
  // Optimized for communication rounds (eigendecomp depends on this)
  void QRFactSquare(Mat<ZZ_p>& Q, Mat<ZZ_p>& R, Mat<ZZ_p>& A);
  // Optimized for memory (need to call this on a large matrix)
  void OrthonormalBasis(Mat<ZZ_p>& Q, Mat<ZZ_p>& A);
  void Tridiag(Mat<ZZ_p>& T, Mat<ZZ_p>& Q, Mat<ZZ_p>& A);
  void EigenDecomp(Mat<ZZ_p>& V, Vec<ZZ_p>& L, Mat<ZZ_p>& A);

  /* Comparison */
  void LessThanPublic(Vec<ZZ_p>& c, Vec<ZZ_p>& a, ZZ_p bpub);
  void NotLessThanPublic(Vec<ZZ_p>& c, Vec<ZZ_p>& a, ZZ_p bpub) {
    LessThanPublic(c, a, bpub);
    FlipBit(c);
  }
  void LessThan(Vec<ZZ_p>& c, Vec<ZZ_p>& a, Vec<ZZ_p>& b);
  void NotLessThan(Vec<ZZ_p>& c, Vec<ZZ_p>& a, Vec<ZZ_p>& b) {
    LessThan(c, a, b);
    FlipBit(c);
  }

  // Test if a number is less than BASE_P / 2
  void IsPositive(Vec<ZZ_p>& b, Vec<ZZ_p>& a);

  // Assumes a is secretly shared binary numbers (0 or 1)
  void FlipBit(Vec<ZZ_p>& a) {
    FlipBit(a, a);
  }
  void FlipBit(Vec<ZZ_p>& b, Vec<ZZ_p>& a);

  // Assumes a is strictly positive and NBIT_K - NBIT_F is even
  void FPSqrt(Vec<ZZ_p>& b, Vec<ZZ_p>& b_inv, Vec<ZZ_p>& a);

  // Assumes b is strictly positive
  void FPDiv(Vec<ZZ_p>& c, Vec<ZZ_p>& a, Vec<ZZ_p>& b);

  // k is the bit-length of the underlying data range
  // m is the number of bits to truncate
  void Trunc(Mat<ZZ_p>& a, int k, int m);
  void Trunc(Vec<ZZ_p>& a, int k, int m) {
    Mat<ZZ_p> am;
    am.SetDims(1, a.length());
    am[0] = a;
    Trunc(am, k, m);
    a = am[0];
  }
  void Trunc(ZZ_p& a, int k, int m) {
    Mat<ZZ_p> am;
    am.SetDims(1, 1);
    am[0][0] = a;
    Trunc(am, k, m);
    a = am[0][0];
  }
  // in most cases, Trunc gets called after a multiplication
  // set default parameters for this case
  void Trunc(Mat<ZZ_p>& a) { Trunc(a, Param::NBIT_K + Param::NBIT_F, Param::NBIT_F); }
  void Trunc(Vec<ZZ_p>& a) { Trunc(a, Param::NBIT_K + Param::NBIT_F, Param::NBIT_F); }
  void Trunc(ZZ_p& a) { Trunc(a, Param::NBIT_K + Param::NBIT_F, Param::NBIT_F); }

  // Returns shares of 2^(2t) where (2t or 2t+1) = NBIT_K - (bit-length of a number in a)
  // b_sqrt contains 2^t
  // Assumes input is strictly positive
  void NormalizerEvenExp(Vec<ZZ_p>& b, Vec<ZZ_p>& b_sqrt, Vec<ZZ_p>& a);

  void LessThanBitsPublic(Vec<ZZ>& c, Mat<ZZ>& a, Mat<ZZ>& b_pub, int fid) {
    LessThanBitsAux(c, a, b_pub, 2, fid);
  }

  void LessThanBits(Vec<ZZ>& c, Mat<ZZ>& a, Mat<ZZ>& b, int fid) {
    LessThanBitsAux(c, a, b, 0, fid);
  }

  // The i-th column of b contains 0,1,...,pow powers of a[i]
  template<class T>
  void Powers(Mat<T>& b, Vec<T>& a, int pow, int fid = 0) {
    if (debug) cout << "Powers: " << a.length() << ", power = " << pow << endl;

    assert(pow >= 1);

    int n = a.length();

    if (pow == 1) {
      Init(b, 2, n);
      if (pid > 0) {
        if (pid == 1) {
          AddScalar(b[0], T(1));
        }
        b[1] = a;
      }
    } else { // pow > 1
      Vec<T> ar, am;
      BeaverPartition(ar, am, a, fid);

      if (pid == 0) {
        Mat<T> ampow;
        ampow.SetDims(pow - 1, n);
        mul_elem(ampow[0], am, am);
        for (int p = 1; p < ampow.NumRows(); p++) {
          mul_elem(ampow[p], ampow[p - 1], am);
          Mod(ampow[p], fid);
        }

        Mat<T> r;
        SwitchSeed(1);
        RandMat(r, pow - 1, n, fid);
        RestoreSeed();

        ampow -= r;
        Mod(ampow, fid);
        r.kill();

        SendMat(ampow, 2, fid);

        b.SetDims(pow + 1, n);
      } else {
        Mat<T> ampow;
        if (pid == 1) {
          SwitchSeed(0);
          RandMat(ampow, pow - 1, n, fid);
          RestoreSeed();
        } else { // pid == 2
          ReceiveMat(ampow, 0, pow - 1, n, fid);
        }

        Mat<T> arpow;
        arpow.SetDims(pow - 1, n);
        mul_elem(arpow[0], ar, ar);
        for (int p = 1; p < arpow.NumRows(); p++) {
          mul_elem(arpow[p], arpow[p - 1], ar);
          Mod(arpow[p], fid);
        }

        Mat<T> t;
        GetPascalMatrix(t, pow, fid);

        Init(b, pow + 1, n);

        if (pid == 1) {
          AddScalar(b[0], T(1));
        }
        b[1] = a;

        Vec<T> tmp;
        for (int p = 2; p <= pow; p++) {
          if (pid == 1) {
            b[p] = arpow[p - 2];
          }

          if (p == 2) {
            mul_elem(tmp, ar, am);
            b[p] += t[p][1] * tmp;
          } else {
            mul_elem(tmp, arpow[p - 3], am);
            b[p] += t[p][1] * tmp;

            for (int j = 2; j <= p - 2; j++) {
              mul_elem(tmp, arpow[p - 2 - j], ampow[j - 2]);
              b[p] += t[p][j] * tmp;
            }

            mul_elem(tmp, ar, ampow[p - 3]);
            b[p] += t[p][p - 1] * tmp;
          }

          b[p] += ampow[p - 2];
        }

        Mod(b, fid);
      }
    }
  }

  template<class T>
  void EvaluatePoly(Mat<T>& b, Vec<T>& a, Mat<T>& coeff, int fid = 0) {
    if (debug) cout << "EvaluatePoly: " << a.length() << ", deg = " << coeff.NumCols() - 1 << endl;

    int n = a.length();
    int npoly = coeff.NumRows();
    int deg = coeff.NumCols() - 1;

    Mat<T> apow;
    Powers(apow, a, deg, fid);

    if (pid > 0) {
      b = coeff * apow;
      Mod(b, fid);
    } else {
      b.SetDims(npoly, n);
    }
  }

  // The i-th element of b contains the OR of i-th row of a
  void FanInOr(Vec<ZZ>& b, Mat<ZZ>& a, int fid);

  // Each row of input a is a binary representation of a number
  // Each row of output b is prefix-OR of the corresponding row of a
  void PrefixOr(Mat<ZZ>& b, Mat<ZZ>& a, int fid);

  template<class T>
  void RevealSym(T& a, int fid = 0) {
    if (pid == 0) {
      return;
    }

    T b;
    if (pid == 1) {
      SendElem(a, 3 - pid, fid);
      ReceiveElem(b, 3 - pid, fid);
    } else {
      ReceiveElem(b, 3 - pid, fid);
      SendElem(a, 3 - pid, fid);
    }

    a += b;
    Mod(a, fid);
  }

  template<class T>
  void RevealSym(Vec<T>& a, int fid = 0) {
    if (debug) cout << "RevealSym: " << a.length() << endl;

    if (pid == 0) {
      return;
    }

    Vec<T> b;
    if (pid == 1) {
      SendVec(a, 3 - pid, fid);
      ReceiveVec(b, 3 - pid, a.length(), fid);
    } else {
      ReceiveVec(b, 3 - pid, a.length(), fid);
      SendVec(a, 3 - pid, fid);
    }

    a += b;
    Mod(a, fid);
  }

  template<class T>
  void RevealSym(Mat<T>& a, int fid = 0) {
    if (debug) cout << "RevealSym: " << a.NumRows() << ", " << a.NumCols() << endl;

    if (pid == 0) {
      return;
    }

    Mat<T> b;
    if (pid == 1) {
      SendMat(a, 3 - pid, fid);
      ReceiveMat(b, 3 - pid, a.NumRows(), a.NumCols(), fid);
    } else {
      ReceiveMat(b, 3 - pid, a.NumRows(), a.NumCols(), fid);
      SendMat(a, 3 - pid, fid);
    }

    a += b;
    Mod(a, fid);
  }

  template<class T>
  void RevealSym(Vec< Mat<T> >& a, int fid = 0) {
    if (debug) {
      cout << "RevealSym" << endl;
    }

    if (pid == 0) {
      return;
    }

    int nmat = a.length();

    Vec<int> nrows, ncols;
    nrows.SetLength(nmat);
    ncols.SetLength(nmat);
    for (int k = 0; k < nmat; k++) {
      nrows[k] = a[k].NumRows();
      ncols[k] = a[k].NumCols();
    }

    Vec< Mat<T> > b;
    if (pid == 1) {
      SendMatParallel(a, 3 - pid, fid);
      ReceiveMatParallel(b, 3 - pid, nrows, ncols, fid);
    } else {
      ReceiveMatParallel(b, 3 - pid, nrows, ncols, fid);
      SendMatParallel(a, 3 - pid, fid);
    }

    for (int k = 0; k < nmat; k++) {
      a[k] += b[k];
      Mod(a[k], fid);
    }
  }

  template<class T>
  void Print(T& a, int fid = 0) {
    Print(a, cout, fid);
  }

  template<class T>
  void PrintFP(T& a) {
    PrintFP(a, cout);
  }

  void PrintFP(ZZ_p& a, ostream& os) {
    Vec<ZZ_p> a_copy;
    a_copy.SetLength(1);
    a_copy[0] = a;
    PrintFP(a_copy, os);
  }

  void PrintFP(Vec<ZZ_p>& a, int maxlen) {
    PrintFP(a, maxlen, cout);
  }

  template<class T>
  void Print(Vec<T>& a, int maxlen, int fid = 0) {
    Vec<T> a_copy = a;
    if (maxlen < a.length()) {
      a_copy.SetLength(maxlen);
    }
    Print(a_copy, cout, fid);
  }

  void PrintBeaverFP(ZZ_p& a, ZZ_p& am) {
    ZZ_p a_copy;
    if (pid == 1) {
      a_copy = am;
    } else if (pid == 2) {
      a_copy = a + am;
    }
    PrintFP(a_copy, cout);
  }

  void PrintBeaverFP(Vec<ZZ_p>& a, Vec<ZZ_p>& am, int maxlen) {
    Vec<ZZ_p> a_copy = a;
    Vec<ZZ_p> am_copy = am;
    if (maxlen < a.length()) {
      a_copy.SetLength(maxlen);
      am_copy.SetLength(maxlen);
    }
    if (pid == 1) {
      a_copy = am_copy;
    } else if (pid == 2) {
      a_copy = a_copy + am_copy;
    }
    PrintFP(a_copy, cout);
  }

  void PrintBeaver(Vec<ZZ_p>& a, Vec<ZZ_p>& am, int maxlen, int fid = 0) {
    Vec<ZZ_p> a_copy = a;
    Vec<ZZ_p> am_copy = am;
    if (maxlen < a.length()) {
      a_copy.SetLength(maxlen);
      am_copy.SetLength(maxlen);
    }
    if (pid == 1) {
      a_copy = am_copy;
    } else if (pid == 2) {
      a_copy = a_copy + am_copy;
    }
    Print(a_copy, cout, fid);
  }

  void PrintFP(Vec<ZZ_p>& a, int maxlen, ostream& os) {
    Vec<ZZ_p> a_copy = a;
    if (a.length() < maxlen) {
      maxlen = a.length();
    }
    a_copy.SetLength(maxlen);
    RevealSym(a_copy);
    Vec<double> ad;
    FPToDouble(ad, a_copy, Param::NBIT_K, Param::NBIT_F);

    if (pid == 2) {
      for (int i = 0; i < ad.length(); i++) {
        os << ad[i];
        if (i == ad.length() - 1) {
          os << endl;
        } else {
          os << '\t';
        }
      }
    }
  }

  void PrintFP(Vec<ZZ_p>& a, ostream& os) {
    Vec<ZZ_p> a_copy = a;
    RevealSym(a_copy);
    Vec<double> ad;
    FPToDouble(ad, a_copy, Param::NBIT_K, Param::NBIT_F);

    if (pid == 2) {
      for (int i = 0; i < ad.length(); i++) {
        os << ad[i];
        if (i == ad.length() - 1) {
          os << endl;
        } else {
          os << '\t';
        }
      }
    }
  }

  void PrintFP(Mat<ZZ_p>& a, int maxrow, int maxcol) {
    PrintFP(a, maxrow, maxcol, cout);
  }
  void PrintFP(Mat<ZZ_p>& a, int maxrow, int maxcol, ostream& os) {
    if (a.NumRows() < maxrow) {
      maxrow = a.NumRows();
    }
    if (a.NumCols() < maxcol) {
      maxcol = a.NumCols();
    }

    Mat<ZZ_p> a_copy;
    a_copy.SetDims(maxrow, maxcol);
    for (int i = 0; i < maxrow; i++) {
      for (int j = 0; j < maxcol; j++) {
        a_copy[i][j] = a[i][j];
      }
    }

    RevealSym(a_copy);
    Mat<double> ad;
    FPToDouble(ad, a_copy, Param::NBIT_K, Param::NBIT_F);

    if (pid == 2) {
      for (int i = 0; i < ad.NumRows(); i++) {
        for (int j = 0; j < ad.NumCols(); j++) {
          os << ad[i][j];
          if (j == ad.NumCols() - 1) {
            os << endl;
          } else {
            os << '\t';
          }
        }
      }
    }
  }

  void PrintFP(Mat<ZZ_p>& a, ostream& os) {
    Mat<ZZ_p> a_copy = a;
    RevealSym(a_copy);
    Mat<double> ad;
    FPToDouble(ad, a_copy, Param::NBIT_K, Param::NBIT_F);

    if (pid == 2) {
      for (int i = 0; i < ad.NumRows(); i++) {
        for (int j = 0; j < ad.NumCols(); j++) {
          os << ad[i][j];
          if (j == ad.NumCols() - 1) {
            os << endl;
          } else {
            os << '\t';
          }
        }
      }
    }
  }

  template<class T>
  void Print(Mat<T>& a, ostream& os, int fid = 0) {
    Mat<T> a_copy = a;
    RevealSym(a_copy, fid);

    if (pid == 2) {
      for (int i = 0; i < a_copy.NumRows(); i++) {
        for (int j = 0; j < a_copy.NumCols(); j++) {
          os << a_copy[i][j];
          if (j == a_copy.NumCols() - 1) {
            os << endl;
          } else {
            os << '\t';
          }
        }
      }
    }
  }

  template<class T>
  void Print(Vec<T>& a, ostream& os, int fid = 0) {
    Vec<T> a_copy = a;
    RevealSym(a_copy, fid);

    if (pid == 2) {
      for (int i = 0; i < a_copy.length(); i++) {
        os << a_copy[i];
        if (i == a_copy.length() - 1) {
          os << endl;
        } else {
          os << '\t';
        }
      }
    }
  }

  void InnerProd(ZZ_p& c, Vec<ZZ_p>& a);
  void InnerProd(Vec<ZZ_p>& c, Mat<ZZ_p>& a); // for each row

  void BeaverReadFromFile(Mat<ZZ_p>& ar, Mat<ZZ_p>& am, ifstream& ifs, int nrow, int ncol);
  void BeaverReadFromFile(Vec<ZZ_p>& ar, Vec<ZZ_p>& am, ifstream& ifs, int n);
  void BeaverReadFromFileWithFilter(Vec<ZZ_p>& ar, Vec<ZZ_p>& am, ifstream& ifs, Vec<ZZ_p>& filt);
  void BeaverWriteToFile(Vec<ZZ_p>& ar, Vec<ZZ_p>& am, fstream& ofs);
  void BeaverWriteToFile(Mat<ZZ_p>& ar, Mat<ZZ_p>& am, fstream& ofs);

  template<class T>
  void BeaverFlipBit(Vec<T>& a, Vec<T>& a_mask) {
    if (pid > 0) {
      a *= -1;
      for (int i = 0; i < a.length(); i++) {
        a[i] += 1;
      }
    }
    a_mask *= -1;
  }

  template<class T>
  void BeaverReconstruct(T& ab, int fid = 0) {
    if (pid == 0) {
      T mask;
      SwitchSeed(1);
      RandElem(mask, fid);
      RestoreSeed();

      ab -= mask;
      Mod(ab, fid);

      SendElem(ab, 2, fid);
    } else {
      T ambm;
      if (pid == 2) {
        ReceiveElem(ambm, 0, fid);
      } else {
        SwitchSeed(0);
        RandElem(ambm, fid);
        RestoreSeed();
      }

      ab += ambm;
      Mod(ab, fid);
    }
  }

  template<class T>
  void BeaverReconstruct(Vec<T>& ab, int fid = 0) {
    if (pid == 0) {
      Vec<T> mask;
      SwitchSeed(1);
      RandVec(mask, ab.length(), fid);
      RestoreSeed();

      ab -= mask;
      Mod(ab, fid);

      SendVec(ab, 2, fid);
    } else {
      Vec<T> ambm;
      if (pid == 2) {
        ReceiveVec(ambm, 0, ab.length(), fid);
      } else {
        SwitchSeed(0);
        RandVec(ambm, ab.length(), fid);
        RestoreSeed();
      }

      ab += ambm;
      Mod(ab, fid);
    }
  }

  template<class T>
  void BeaverReconstruct(Mat<T>& ab, int fid = 0) {
    if (pid == 0) {
      Mat<T> mask;
      SwitchSeed(1);
      RandMat(mask, ab.NumRows(), ab.NumCols(), fid);
      RestoreSeed();

      ab -= mask;
      Mod(ab, fid);

      SendMat(ab, 2, fid);
    } else {
      Mat<T> ambm;
      if (pid == 2) {
        ReceiveMat(ambm, 0, ab.NumRows(), ab.NumCols(), fid);
      } else {
        SwitchSeed(0);
        RandMat(ambm, ab.NumRows(), ab.NumCols(), fid);
        RestoreSeed();
      }

      ab += ambm;
      Mod(ab, fid);
    }
  }

  template<class T>
  void BeaverReconstruct(Vec< Mat<T> >& ab, int fid = 0) {
    int nmat = ab.length();

    if (pid == 0) {
      Mat<T> mask;

      SwitchSeed(1);
      for (int i = 0; i < nmat; i++) {
        RandMat(mask, ab[i].NumRows(), ab[i].NumCols(), fid);
        ab[i] -= mask;
      }
      RestoreSeed();

      Mod(ab, fid);

      SendMatParallel(ab, 2, fid);
    } else {
      Vec< Mat<T> > ambm;

      if (pid == 2) {
        Vec<int> nrows, ncols;
        nrows.SetLength(nmat);
        ncols.SetLength(nmat);
        for (int i = 0; i < nmat; i++) {
          nrows[i] = ab[i].NumRows();
          ncols[i] = ab[i].NumCols();
        }

        ReceiveMatParallel(ambm, 0, nrows, ncols, fid);
      } else {
        ambm.SetLength(nmat);

        SwitchSeed(0);
        for (int i = 0; i < nmat; i++) {
          RandMat(ambm[i], ab[i].NumRows(), ab[i].NumCols(), fid);
        }
        RestoreSeed();
      }

      for (int i = 0; i < nmat; i++) {
        ab[i] += ambm[i];
      }

      Mod(ab, fid);
    }
  }

  template<class T>
  void BeaverMult(T& ab, T& ar, T& am, T& br, T& bm, int fid = 0) {
    if (pid == 0) {
      ab += am * bm;
    } else {
      ab += ar * bm;
      ab += am * br;
      if (pid == 1) {
        ab += ar * br;
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverMult(Vec<T>& ab, Vec<T>& ar, Vec<T>& am, T& br, T& bm, int fid = 0) {
    if (pid == 0) {
      ab += am * bm;
    } else {
      ab += ar * bm;
      ab += am * br;
      if (pid == 1) {
        ab += ar * br;
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverMult(Vec<T>& ab, Mat<T>& ar, Mat<T>& am, Vec<T>& br, Vec<T>& bm, int fid = 0) {
    if (pid == 0) {
      ab += am * bm;
    } else {
      ab += ar * bm;
      ab += am * br;
      if (pid == 1) {
        ab += ar * br;
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverMult(Vec<T>& ab, Vec<T>& ar, Vec<T>& am, Mat<T>& br, Mat<T>& bm, int fid = 0) {
    if (pid == 0) {
      ab += am * bm;
    } else {
      ab += ar * bm;
      ab += am * br;
      if (pid == 1) {
        ab += ar * br;
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverMultMat(Mat<T>& ab, Mat<T>& ar, Mat<T>& am, Mat<T>& br, Mat<T>& bm, int fid = 0) {
    BeaverMult(ab, ar, am, br, bm, false, fid);
  }

  void BeaverMult(Mat<ZZ>& ab, Mat<ZZ>& ar, Mat<ZZ>& am, Mat<ZZ>& br, Mat<ZZ>& bm, bool elem_wise, int fid);
  void BeaverMult(Mat<ZZ_p>& ab, Mat<ZZ_p>& ar, Mat<ZZ_p>& am, Mat<ZZ_p>& br, Mat<ZZ_p>& bm, bool elem_wise, int fid = 0);

  void BeaverMultElem(Vec<ZZ>& ab, Vec<ZZ>& ar, Vec<ZZ>& am, Vec<ZZ>& br, Vec<ZZ>& bm, int fid);
  void BeaverMultElem(Vec<ZZ_p>& ab, Vec<ZZ_p>& ar, Vec<ZZ_p>& am, Vec<ZZ_p>& br, Vec<ZZ_p>& bm, int fid = 0);

  template<class T>
  void BeaverMultElem(Mat<T>& ab, Mat<T>& ar, Mat<T>& am, Mat<T>& br, Mat<T>& bm, int fid = 0) {
    BeaverMult(ab, ar, am, br, bm, true, fid);
  }

  template<class T>
  void BeaverInnerProd(T& ab, Vec<T>& ar, Vec<T>& am, Vec<T>& br, Vec<T>& bm, int fid = 0) {
    for (int i = 0; i < ar.length(); i++) {
      if (pid == 0) {
        ab += am[i] * bm[i];
      } else {
        ab += ar[i] * bm[i];
        ab += br[i] * am[i];
        if (pid == 1) {
          ab += ar[i] * br[i];
        }
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverInnerProd(T& ab, Vec<T>& ar, Vec<T>& am, int fid = 0) {
    for (int i = 0; i < ar.length(); i++) {
      if (pid == 0) {
        ab += am[i] * am[i];
      } else {
        ab += 2 * ar[i] * am[i];
        if (pid == 1) {
          ab += ar[i] * ar[i];
        }
      }
    }

    Mod(ab, fid);
  }

  template<class T>
  void BeaverPartition(T& am, T& a, int fid = 0) {
    BeaverPartition(a, am, a, fid);
  }

  template<class T>
  void BeaverPartition(Vec<T>& am, Vec<T>& a, int fid = 0) {
    BeaverPartition(a, am, a, fid);
  }

  template<class T>
  void BeaverPartition(Mat<T>& am, Mat<T>& a, int fid = 0) {
    BeaverPartition(a, am, a, fid);
  }

  template<class T>
  void BeaverPartition(Vec< Mat<T> >& am, Vec< Mat<T> >& a, int fid = 0) {
    BeaverPartition(a, am, a, fid);
  }

  template<class T>
  void BeaverPartition(T& ar, T& am, T& a, int fid = 0) {
    if (pid == 0) {
      T x1;
      SwitchSeed(1);
      RandElem(x1, fid);
      RestoreSeed();

      T x2;
      SwitchSeed(2);
      RandElem(x2, fid);
      RestoreSeed();

      am = x1 + x2;
      Mod(am, fid);
    } else {
      SwitchSeed(0);
      RandElem(am, fid);
      RestoreSeed();

      ar = a - am;
      Mod(ar, fid);
      RevealSym(ar, fid);
    }
  }

  template<class T>
  void BeaverPartition(Vec<T>& ar, Vec<T>& am, Vec<T>& a, int fid = 0) {
    int n = a.length();

    if (pid == 0) {
      Vec<T> x1;
      SwitchSeed(1);
      RandVec(x1, n, fid);
      RestoreSeed();

      Vec<T> x2;
      SwitchSeed(2);
      RandVec(x2, n, fid);
      RestoreSeed();

      am = x1 + x2;
      Mod(am, fid);
      ar.SetLength(n);
    } else {
      SwitchSeed(0);
      RandVec(am, n, fid);
      RestoreSeed();

      ar = a - am;
      Mod(ar, fid);
      RevealSym(ar, fid);
    }
  }

  template<class T>
  void BeaverPartition(Mat<T>& ar, Mat<T>& am, Mat<T>& a, int fid = 0) {
    if (debug) cout << "BeaverPartition: " << a.NumRows() << ", " << a.NumCols() << endl;

    int nrow = a.NumRows();
    int ncol = a.NumCols();

    if (pid == 0) {
      Mat<T> x1;
      SwitchSeed(1);
      RandMat(x1, nrow, ncol, fid);
      RestoreSeed();

      Mat<T> x2;
      SwitchSeed(2);
      RandMat(x2, nrow, ncol, fid);
      RestoreSeed();

      am = x1 + x2;
      Mod(am, fid);
      ar.SetDims(nrow, ncol);
    } else {
      SwitchSeed(0);
      RandMat(am, nrow, ncol, fid);
      RestoreSeed();

      ar = a - am;
      Mod(ar, fid);
      RevealSym(ar, fid);
    }
  }

  template<class T>
  void BeaverPartition(Vec< Mat<T> >& ar, Vec< Mat<T> >& am, Vec< Mat<T> >& a, int fid = 0) {
    bool inplace = &ar == &a;

    int nmat = a.length();
    am.SetLength(nmat);
    if (!inplace) {
      ar.SetLength(nmat);
    }

    if (pid == 0) {
      ar.SetLength(nmat);

      for (int i = 0; i < nmat; i++) {
        Mat<T> x1;
        SwitchSeed(1);
        RandMat(x1, a[i].NumRows(), a[i].NumCols(), fid);
        RestoreSeed();

        Mat<T> x2;
        SwitchSeed(2);
        RandMat(x2, a[i].NumRows(), a[i].NumCols(), fid);
        RestoreSeed();

        am[i] = x1 + x2;
        Mod(am[i], fid);

        if (!inplace) {
          ar[i].SetDims(a[i].NumRows(), a[i].NumCols());
        }
      }
    } else {
      for (int i = 0; i < nmat; i++) {
        SwitchSeed(0);
        RandMat(am[i], a[i].NumRows(), a[i].NumCols(), fid);
        RestoreSeed();

        ar[i] = a[i] - am[i];
        Mod(ar[i], fid);
      }

      RevealSym(ar, fid);
    }
  }

  template<class T>
  void AddPublic(Vec<T>& a, T b) {
    if (pid == 1) {
      AddScalar(a, b);
    }
  }

  template<class T>
  void AddPublic(Mat<T>& a, T b) {
    if (pid == 1) {
      AddScalar(a, b);
    }
  }

  template<class T>
  void Add(Vec<T>& a, T b) {
    if (pid > 0) {
      AddScalar(a, b);
    }
  }

  template<class T>
  void Add(Mat<T>& a, T b) {
    if (pid > 0) {
      AddScalar(a, b);
    }
  }

  template<class T>
  void MultMat(Vec<T>& c, Mat<T>& a, Vec<T>& b, int fid = 0) {
    Mat<T> ar, am;
    Vec<T> br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    Init(c, a.NumRows());
    BeaverMult(c, ar, am, br, bm, fid);

    BeaverReconstruct(c, fid);
  }

  template<class T>
  void MultMat(Vec<T>& c, Vec<T>& a, Mat<T>& b, int fid = 0) {
    Vec<T> ar, am;
    Mat<T> br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    Init(c, a.length());
    BeaverMult(c, ar, am, br, bm, fid);

    BeaverReconstruct(c, fid);
  }

  template<class T>
  void MultMat(Mat<T>& c, Mat<T>& a, Mat<T>& b, int fid = 0) {
    MultAux(c, a, b, false, fid);
  }

  template<class T>
  void MultMatParallel(Vec< Mat<T> >& c, Vec< Mat<T> >& a, Vec< Mat<T> >& b, int fid = 0) {
    MultAuxParallel(c, a, b, false, fid);
  }

  template<class T>
  void MultMat(Vec<T>& c, Vec<T>& a, T& b, int fid = 0) {
    Vec<T> ar, am;
    T br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    Init(c, a.length());
    BeaverMult(c, ar, am, br, bm, fid);

    BeaverReconstruct(c, fid);
  }

  template<class T>
  void MultElem(Vec<T>& c, Vec<T>& a, Vec<T>& b, int fid = 0) {
    Vec<T> ar, am, br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    Init(c, a.length());
    BeaverMultElem(c, ar, am, br, bm, fid);

    BeaverReconstruct(c, fid);
  }

  template<class T>
  void MultElem(Mat<T>& c, Mat<T>& a, Mat<T>& b, int fid = 0) {
    MultAux(c, a, b, true, fid);
  }

  template<class T>
  void MultElemParallel(Vec< Mat<T> >& c, Vec< Mat<T> >& a, Vec< Mat<T> >& b, int fid = 0) {
    MultAuxParallel(c, a, b, true, fid);
  }

  template<class T>
  void Reshape(Mat<T>& b, T& a) {
    b.SetDims(1, 1);
    b[0][0] = a;
  }

  template<class T>
  void Reshape(Vec<T>& b, Mat<T>& a) {
    b.SetLength(a.NumRows() * a.NumCols());
    if (pid > 0) {
      int ind = 0;
      for (int i = 0; i < a.NumRows(); i++) {
        for (int j = 0; j < a.NumCols(); j++) {
          b[ind] = a[i][j];
          ind++;
        }
      }
    }
  }

  template<class T>
  void Reshape(Mat<T>& a, int nrows, int ncols) {
    if (pid == 0) {
      assert(a.NumRows() * a.NumCols() == nrows * ncols);
      a.SetDims(nrows, ncols);
    } else {
      ReshapeMat(a, nrows, ncols);
    }
  }

  template<class T>
  void Reshape(Mat<T>& b, Vec<T>& a, int nrows, int ncols) {
    if (pid == 0) {
      assert(a.length() == nrows * ncols);
      b.SetDims(nrows, ncols);
    } else {
      ReshapeMat(b, a, nrows, ncols);
    }
  }

  template<class T>
  void Transpose(Mat<T>& a) {
    Transpose(a, a);
  }

  template<class T>
  void Transpose(Mat<T>& b, Mat<T>& a) {
    if (pid == 0) {
      b.SetDims(a.NumCols(), a.NumRows());
    } else {
      b = transpose(a);
    }
  }

  template<class T>
  void FilterCols(Mat<ZZ_p>& a, Vec<T>& filt, int new_ncols) {
    if (pid > 0) {
      FilterMatCols(a, filt);
    } else {
      a.SetDims(a.NumRows(), new_ncols);
    }
  }

  template<class T>
  void FilterRows(Mat<ZZ_p>& a, Vec<T>& filt, int new_nrows) {
    if (pid > 0) {
      FilterMatRows(a, filt);
    } else {
      a.SetDims(new_nrows, a.NumCols());
    }
  }

  template<class T>
  void Filter(Vec<ZZ_p>& a, Vec<T>& filt, int new_len) {
    if (pid > 0) {
      FilterVec(a, filt);
    } else {
      a.SetLength(new_len);
    }
  }

  /* File IO */
  void ReadFromFile(ZZ_p& a, ifstream& ifs);
  void ReadFromFile(Vec<ZZ_p>& a, ifstream& ifs, int n);
  void ReadFromFile(Mat<ZZ_p>& a, ifstream& ifs, int nrow, int ncol);
  void WriteToFile(ZZ_p& a, fstream& ofs);
  void WriteToFile(Vec<ZZ_p>& a, fstream& ofs);
  void WriteToFile(Mat<ZZ_p>& a, fstream& ofs);
  void SkipData(ifstream& ifs, int n);
  void SkipData(ifstream& ifs, int nrows, int ncols);

  /* Communication */

  template<class T>
  void ReceiveElem(T& a, int from_pid, int fid = 0) {
    unsigned char *buf_ptr = buf;
    sockets.find(from_pid)->second.ReceiveSecure(buf, ZZ_bytes[fid]);
    ConvertBytes(a, buf_ptr, fid);
  }

  template<class T>
  void ReceiveVec(Vec<T>& a, int from_pid, int n, int fid = 0) {
    a.SetLength(n);
    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    uint64_t remaining = n;
    for (uint64_t i = 0; i < n; i++) {
      if (stored_in_buf == 0) {
        uint64_t count;
        if (remaining < ZZ_per_buf[fid]) {
          count = remaining;
        } else {
          count = ZZ_per_buf[fid];
        }
        sockets.find(from_pid)->second.ReceiveSecure(buf, count * ZZ_bytes[fid]);
        stored_in_buf += count;
        remaining -= count;
        buf_ptr = buf;
      }

      ConvertBytes(a[i], buf_ptr, fid);
      buf_ptr += ZZ_bytes[fid];
      stored_in_buf--;
    }
  }

  template<class T>
  void ReceiveMat(Mat<T>& a, int from_pid, int nrows, int ncols, int fid = 0) {
    a.SetDims(nrows, ncols);
    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    uint64_t remaining = nrows * ncols;
    for (int i = 0; i < a.NumRows(); i++) {
      for (int j = 0; j < a.NumCols(); j++) {
        if (stored_in_buf == 0) {
          uint64_t count;
          if (remaining < ZZ_per_buf[fid]) {
            count = remaining;
          } else {
            count = ZZ_per_buf[fid];
          }
          sockets.find(from_pid)->second.ReceiveSecure(buf, count * ZZ_bytes[fid]);
          stored_in_buf += count;
          remaining -= count;
          buf_ptr = buf;
        }

        ConvertBytes(a[i][j], buf_ptr, fid);
        buf_ptr += ZZ_bytes[fid];
        stored_in_buf--;
      }
    }
  }

  template<class T>
  void SendElem(T& a, int to_pid, int fid = 0) {
    unsigned char *buf_ptr = buf;
    BytesFromZZ(buf_ptr, AsZZ(a), ZZ_bytes[fid]);
    sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid]);
  }

  template<class T>
  void SendVec(Vec<T>& a, int to_pid, int fid = 0) {
    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    for (int i = 0; i < a.length(); i++) {
      if (stored_in_buf == ZZ_per_buf[fid]) {
        sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
        stored_in_buf = 0;
        buf_ptr = buf;
      }

      BytesFromZZ(buf_ptr, AsZZ(a[i]), ZZ_bytes[fid]);
      stored_in_buf++;
      buf_ptr += ZZ_bytes[fid];
    }

    if (stored_in_buf > 0) {
      sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
    }
  }

  template<class T>
  void SendMat(Mat<T>& a, int to_pid, int fid = 0) {
    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    for (int i = 0; i < a.NumRows(); i++) {
      for (int j = 0; j < a.NumCols(); j++) {
        if (stored_in_buf == ZZ_per_buf[fid]) {
          sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
          stored_in_buf = 0;
          buf_ptr = buf;
        }

        BytesFromZZ(buf_ptr, AsZZ(a[i][j]), ZZ_bytes[fid]);
        stored_in_buf++;
        buf_ptr += ZZ_bytes[fid];
      }
    }

    if (stored_in_buf > 0) {
      sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
    }
  }

  template<class T>
  void ReceiveMatParallel(Vec< Mat<T> >& a, int from_pid, Vec<int>& nrows, Vec<int>& ncols, int fid = 0) {
    assert(nrows.length() == ncols.length());

    int nmat = nrows.length();
    a.SetLength(nmat);

    uint64_t remaining = 0;
    for (int k = 0; k < nmat; k++) {
      remaining += nrows[k] * ncols[k];
    }

    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    for (int k = 0; k < nmat; k++) {
      a[k].SetDims(nrows[k], ncols[k]);
      for (int i = 0; i < nrows[k]; i++) {
        for (int j = 0; j < ncols[k]; j++) {
          if (stored_in_buf == 0) {
            uint64_t count;
            if (remaining < ZZ_per_buf[fid]) {
              count = remaining;
            } else {
              count = ZZ_per_buf[fid];
            }
            sockets.find(from_pid)->second.ReceiveSecure(buf, count * ZZ_bytes[fid]);
            stored_in_buf += count;
            remaining -= count;
            buf_ptr = buf;
          }

          ConvertBytes(a[k][i][j], buf_ptr, fid);
          buf_ptr += ZZ_bytes[fid];
          stored_in_buf--;
        }
      }
    }
  }

  template<class T>
  void SendMatParallel(Vec< Mat<T> >& a, int to_pid, int fid = 0) {
    unsigned char *buf_ptr = buf;
    uint64_t stored_in_buf = 0;
    for (int k = 0; k < a.length(); k++) {
      for (int i = 0; i < a[k].NumRows(); i++) {
        for (int j = 0; j < a[k].NumCols(); j++) {
          if (stored_in_buf == ZZ_per_buf[fid]) {
            sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
            stored_in_buf = 0;
            buf_ptr = buf;
          }

          BytesFromZZ(buf_ptr, AsZZ(a[k][i][j]), ZZ_bytes[fid]);
          stored_in_buf++;
          buf_ptr += ZZ_bytes[fid];
        }
      }
    }

    if (stored_in_buf > 0) {
      sockets.find(to_pid)->second.SendSecure(buf, ZZ_bytes[fid] * stored_in_buf);
    }
  }

  void SendInt(int num, int to_pid);
  int ReceiveInt(int from_pid);

  void SendBool(bool flag, int to_pid);
  bool ReceiveBool(int from_pid);

  void SwitchSeed(int pid);
  void RestoreSeed() {
    // cerr << "[restore] ";
    SwitchSeed(pid);
  }

  void ExportSeed(fstream& ofs);
  void ExportSeed(fstream& ofs, int pid);
  void ImportSeed(int newid, ifstream& ifs);

  void SetDebug(bool flag) {
    debug = flag;
  }

  void RandElem(ZZ& a, int fid) {
    RandomBnd(a, primes[fid]);
  }

  static void RandElem(ZZ_p& a, int fid = 0) {
    random(a);
  }

  void RandVec(Vec<ZZ>& a, int n, int fid) {
    a.SetLength(n);
    for (int i = 0; i < n; i++)
      RandomBnd(a[i], primes[fid]);
  }

  static void RandVec(Vec<ZZ_p>& a, int n, int fid = 0) {
    a.SetLength(n);
    for (int i = 0; i < n; i++)
      random(a[i]);
  }

  void RandMat(Mat<ZZ>& a, int nrows, int ncols, int fid) {
    a.SetDims(nrows, ncols);
    for (int i = 0; i < nrows; i++)
      for (int j = 0; j < ncols; j++)
        RandomBnd(a[i][j], primes[fid]);
  }

  static void RandMat(Mat<ZZ_p>& a, int nrows, int ncols, int fid = 0) {
    a.SetDims(nrows, ncols);
    for (int i = 0; i < nrows; i++)
      for (int j = 0; j < ncols; j++)
        random(a[i][j]);
  }

  static void RandMatBits(Mat<ZZ_p>& a, int nrows, int ncols, int bitlen) {
    a.SetDims(nrows, ncols);
    for (int i = 0; i < nrows; i++)
      for (int j = 0; j < ncols; j++)
        a[i][j] = conv<ZZ_p>(RandomBits_ZZ(bitlen));
  }

  // a contains column indices (1-based) into cached tables
  void TableLookup(Mat<ZZ_p>& b, Vec<ZZ_p>& a, int cache_id);
  void TableLookup(Mat<ZZ_p>& b, Vec<ZZ>& a, int cache_id, int fid);


public:
  map<int, CSocket> sockets;
  map<int, RandomStream> prg;

  /* Table lookup cache */
  Vec< Mat<ZZ_p> > table_cache;
  Vec< bool > table_type_ZZ; // true if indexed by ZZ, false if ZZ_p is used
  Vec< int > table_field_index;
  Vec< Mat<ZZ_p> > lagrange_cache;
  map<int, ZZ_p> invpow_cache;

  /* Profiler data */
  stack<string> pstate;
  map<string, int> ptable_index;
  vector< pair<string, vector<uint64_t> > > ptable;

  /* Lagrange coefficient cache for OR functions */
  map<pair<int, int>, Vec<ZZ> > or_lagrange_cache;

  /* Pascal matrix cache for Powers */
  map<pair<int, int>, Mat<ZZ> > pascal_cache_ZZ;
  map<int, Mat<ZZ_p> > pascal_cache_ZZp;

  int pid;
  int cur_prg_pid;
  unsigned char *buf;
  bool debug;

  Vec<ZZ> primes;
  Vec<uint32_t> ZZ_bytes;
  Vec<uint32_t> ZZ_bits;
  Vec<uint64_t> ZZ_per_buf;

  /* Logging */
  ofstream logfs;
  chrono::time_point<chrono::steady_clock> clock_start;

  /* File IO */
  void Write(Vec<ZZ_p>& a, fstream& ofs);
  void Write(Mat<ZZ_p>& a, fstream& ofs);

  void Read(Vec<ZZ_p>& a, ifstream& ifs, int n);
  void ReadWithFilter(Vec<ZZ_p>& a, ifstream& ifs, Vec<ZZ_p>& filt);
  void Read(Mat<ZZ_p>& a, ifstream& ifs, int nrows, int ncols);

  template<class T>
  void MultAux(Mat<T>& c, Mat<T>& a, Mat<T>& b, bool elem_wise, int fid = 0) {
    if (debug) cout << "MultAux: (" << a.NumRows() << ", " << a.NumCols() << "), (" << b.NumRows() << ", " << b.NumCols() << ")" << endl;
    if (elem_wise) {
      assert(a.NumRows() == b.NumRows() && a.NumCols() == b.NumCols());
    } else {
      assert(a.NumCols() == b.NumRows());
    }

    int out_rows = a.NumRows();
    int out_cols = elem_wise ? a.NumCols() : b.NumCols();

    Mat<T> ar, am, br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    Init(c, out_rows, out_cols);
    BeaverMult(c, ar, am, br, bm, elem_wise, fid);

    BeaverReconstruct(c, fid);
  }

  template<class T>
  void MultAuxParallel(Vec< Mat<T> >& c, Vec< Mat<T> >& a, Vec< Mat<T> >& b, bool elem_wise, int fid = 0) {
    if (debug) cout << "MultAuxParallel" << endl;
    assert(a.length() == b.length());
    int nmat = a.length();

    Vec<int> out_rows, out_cols;
    out_rows.SetLength(nmat);
    out_cols.SetLength(nmat);

    for (int k = 0; k < nmat; k++) {
      if (elem_wise) {
        assert(a[k].NumRows() == b[k].NumRows() && a[k].NumCols() == b[k].NumCols());
      } else {
        assert(a[k].NumCols() == b[k].NumRows());
      }

      out_rows[k] = a[k].NumRows();
      out_cols[k] = elem_wise ? a[k].NumCols() : b[k].NumCols();
    }

    Vec< Mat<T> > ar, am, br, bm;
    BeaverPartition(ar, am, a, fid);
    BeaverPartition(br, bm, b, fid);

    c.SetLength(nmat);
    for (int k = 0; k < nmat; k++) {
      Init(c[k], out_rows[k], out_cols[k]);
      BeaverMult(c[k], ar[k], am[k], br[k], bm[k], elem_wise, fid);
    }

    BeaverReconstruct(c, fid);
  }

  void LessThanBitsAux(Vec<ZZ>& c, Mat<ZZ>& a, Mat<ZZ>& b, int public_flag, int fid);

  // k is the bit-length of the underlying data range
  // outputs n random numbers (and their bit-representations)
  void ShareRandomBits(Vec<ZZ_p>& r, Mat<ZZ>& rbits, int k, int n, int fid);

  const ZZ& AsZZ(ZZ& val) {
    return val;
  }

  const ZZ& AsZZ(ZZ_p& val) {
    return rep(val);
  }

  void ConvertBytes(ZZ& a, unsigned char *buf, int fid) {
    a = ZZFromBytes(buf, ZZ_bytes[fid]);
  }

  void ConvertBytes(ZZ_p& a, unsigned char *buf, int fid = 0) {
    a = conv<ZZ_p>(ZZFromBytes(buf, ZZ_bytes[fid]));
  }

  void Mod(ZZ_p& a, int fid = 0) {}
  void Mod(Vec<ZZ_p>& a, int fid = 0) {}
  void Mod(Mat<ZZ_p>& a, int fid = 0) {}
  void Mod(Vec< Mat<ZZ_p> >& a, int fid = 0) {}

  void Mod(ZZ& a, int fid) {
    a %= primes[fid];
  }

  void Mod(Vec<ZZ>& a, int fid) {
    for (int i = 0; i < a.length(); i++) {
      a[i] %= primes[fid];
    }
  }

  void Mod(Mat<ZZ>& a, int fid) {
    for (int i = 0; i < a.NumRows(); i++) {
      for (int j = 0; j < a.NumCols(); j++) {
        a[i][j] %= primes[fid];
      }
    }
  }

  void Mod(Vec< Mat<ZZ> >& a, int fid) {
    for (int k = 0; k < a.length(); k++) {
      for (int i = 0; i < a[k].NumRows(); i++) {
        for (int j = 0; j < a[k].NumCols(); j++) {
          a[k][i][j] %= primes[fid];
        }
      }
    }
  }

  void mul_elem(Vec<ZZ_p>& c, Vec<ZZ_p>& a, Vec<ZZ_p>& b) {
    assert(a.length() == b.length());
    c.SetLength(a.length());

    ZZ_pContext context;
    context.save();

    NTL_GEXEC_RANGE(c.length() > Param::PAR_THRES, c.length(), first, last)

    context.restore();

    for (int i = first; i < last; i++) {
      mul(c[i], a[i], b[i]);
    }

    NTL_GEXEC_RANGE_END
  }

  void mul_elem(Vec<ZZ>& c, Vec<ZZ>& a, Vec<ZZ>& b) {
    assert(a.length() == b.length());
    c.SetLength(a.length());

    NTL_GEXEC_RANGE(c.length() > Param::PAR_THRES, c.length(), first, last)

    for (int i = first; i < last; i++) {
      mul(c[i], a[i], b[i]);
    }

    NTL_GEXEC_RANGE_END
  }

  void mul_elem(Mat<ZZ_p>& c, Mat<ZZ_p>& a, Mat<ZZ_p>& b) {
    assert(a.NumRows() == b.NumRows() && a.NumCols() == b.NumCols());
    c.SetDims(a.NumRows(), a.NumCols());

    ZZ_pContext context;
    context.save();

    NTL_GEXEC_RANGE(c.NumRows() > Param::PAR_THRES, c.NumRows(), first, last)

    context.restore();

    for (int i = first; i < last; i++) {
      for (int j = 0; j < c.NumCols(); j++) {
        mul(c[i][j], a[i][j], b[i][j]);
      }
    }

    NTL_GEXEC_RANGE_END
  }

  void mul_elem(Mat<ZZ>& c, Mat<ZZ>& a, Mat<ZZ>& b) {
    assert(a.NumRows() == b.NumRows() && a.NumCols() == b.NumCols());
    c.SetDims(a.NumRows(), a.NumCols());

    NTL_GEXEC_RANGE(c.NumRows() > Param::PAR_THRES, c.NumRows(), first, last)

    for (int i = first; i < last; i++) {
      for (int j = 0; j < c.NumCols(); j++) {
        mul(c[i][j], a[i][j], b[i][j]);
      }
    }

    NTL_GEXEC_RANGE_END
  }

  template<class T>
  void NumToBits(Mat<T>& b, Vec<ZZ_p>& a, int bitlen) {
    b.SetDims(a.length(), bitlen);
    for (int i = 0; i < a.length(); i++)
      for (int j = 0; j < bitlen; j++)
        b[i][j] = bit(rep(a[i]), bitlen - 1 - j);
  }

  void RandMatBits(Mat<ZZ>& a, int nrows, int ncols, int bitlen) {
    a.SetDims(nrows, ncols);
    for (int i = 0; i < nrows; i++)
      for (int j = 0; j < ncols; j++)
        a[i][j] = RandomBits_ZZ(bitlen);
  }

  template<class T>
  void RandVecBits(Vec<T>& a, int n, int bitlen) {
    Mat<T> am;
    RandMatBits(am, 1, n, bitlen);
    a = am[0];
  }


  void GetPascalMatrix(Mat<ZZ>& t, int pow, int fid) {
    pair<int, int> key = make_pair(pow, fid);
    map<pair<int, int>, Mat<ZZ> >::iterator it = pascal_cache_ZZ.find(key);
    if (it != pascal_cache_ZZ.end()) {
      t = it->second;
    } else {
      pascal_matrix(t, pow, fid);
      pascal_cache_ZZ[key] = t;
    }
  }

  void GetPascalMatrix(Mat<ZZ_p>& t, int pow, int fid = 0) {
    int key = pow;
    map<int, Mat<ZZ_p> >::iterator it = pascal_cache_ZZp.find(key);
    if (it != pascal_cache_ZZp.end()) {
      t = it->second;
    } else {
      pascal_matrix(t, pow);
      pascal_cache_ZZp[key] = t;
    }
  }

  template<class T>
  void pascal_matrix(Mat<T>& t, int pow, int fid = 0) {
    t.SetDims(pow + 1, pow + 1);
    for (int i = 0; i < pow + 1; i++) {
      for (int j = 0; j < pow + 1; j++) {
        if (j > i) {
          t[i][j] = 0;
        } else if (j == 0 || j == i) {
          t[i][j] = 1;
        } else {
          t[i][j] = t[i-1][j-1] + t[i-1][j];
          Mod(t[i][j], fid);
        }
      }
    }
  }

  void Inverse(ZZ_p& b, ZZ_p& a, int fid = 0) {
    inv(b, a);
  }

  void Inverse(ZZ& b, ZZ&a, int fid) {
    InvMod(b, a, primes[fid]);
  }

  template<class T>
  void lagrange_interp(Vec<T>& c, Vec<long>& x, Vec<T>& y, int fid = 0) {
    if (debug) cout << "lagrange_interp: " << x.length() << endl;

    assert(x.length() == y.length());

    int n = y.length();

    map<long, T> inv_table;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        if (i == j) {
          continue;
        }

        long key = abs(x[i] - x[j]);
        if (inv_table.find(key) == inv_table.end()) {
          T val;
          T key2(key);
          Inverse(val, key2, fid);
          inv_table[key] = val;
        }
      }
    }

    Mat<T> numer;
    numer.SetDims(n, n);

    Vec<T> denom_inv;
    denom_inv.SetLength(n);

    /* Initialize numer and denom_inv */
    for (int i = 0; i < n; i++) {
      denom_inv[i] = 1;
      if (i == 0) {
        numer[0] = y;
      } else {
        clear(numer[i]);
      }
    }

    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        if (i == j) {
          continue;
        }

        for (int k = n - 1; k >= 0; k--) {
          numer[k][j] = ((k == 0) ? T(0) : numer[k-1][j]) - x[i] * numer[k][j];
          Mod(numer[k][j], fid);
        }
        denom_inv[i] *= ((x[i] > x[j]) ? 1 : -1) * inv_table[abs(x[i] - x[j])];
        Mod(denom_inv[i], fid);
      }
    }

    mul(c, numer, denom_inv);
    Mod(c, fid);
  }

  void lagrange_interp_simple(Vec<ZZ>& c, Vec<ZZ>& y, int fid) {
    int n = y.length();

    Vec<long> x;
    x.SetLength(n);
    for (int i = 0; i < n; i++) {
      x[i] = i + 1;
    }

    lagrange_interp(c, x, y, fid);
  }
};

#endif
