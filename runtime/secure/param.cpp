#include "param.h"
#include <fstream>
#include <sstream>

template<class T>
bool Param::Convert(string s, T &var, string name) {
  istringstream iss(s);
  iss >> std::skipws >> var >> std::skipws;
  if (iss.tellg() != -1) {
    cout << "Parameter parse error: " << name << endl;
    return false;
  }
  return true;
}

bool Param::ParseFile(const char *param_file) {
  ifstream pfile(param_file);
  if (!pfile.is_open()) {
    cout << "Failed to open parameter file: " << param_file << endl;
    return false;
  } else {
    cout << "Using parameters in " << param_file << endl;
  }

  string k, v;
  while (pfile >> std::skipws >> k) {
    if (k[0] == '#') { // Comments
      getline(pfile, v);
      continue;
    }

    getline(pfile, v);

    bool ret;
    if (k == "PORT_P0_P1") {
      ret = Convert(v, Param::PORT_P0_P1, k);
    } else if (k == "PORT_P0_P2") {
      ret = Convert(v, Param::PORT_P0_P2, k);
    } else if (k == "PORT_P1_P2") {
      ret = Convert(v, Param::PORT_P1_P2, k);
    } else if (k == "PORT_P1_P3") {
      ret = Convert(v, Param::PORT_P1_P3, k);
    } else if (k == "PORT_P2_P3") {
      ret = Convert(v, Param::PORT_P2_P3, k);
    } else if (k == "IP_ADDR_P0") {
      ret = Convert(v, Param::IP_ADDR_P0, k);
    } else if (k == "IP_ADDR_P1") {
      ret = Convert(v, Param::IP_ADDR_P1, k);
    } else if (k == "IP_ADDR_P2") {
      ret = Convert(v, Param::IP_ADDR_P2, k);
    } else if (k == "KEY_PATH") {
      ret = Convert(v, Param::KEY_PATH, k);
    } else if (k == "NBIT_K") {
      ret = Convert(v, Param::NBIT_K, k);
    } else if (k == "NBIT_F") {
      ret = Convert(v, Param::NBIT_F, k);
    } else if (k == "NBIT_V") {
      ret = Convert(v, Param::NBIT_V, k);
    } else if (k == "BASE_P") {
      ret = Convert(v, Param::BASE_P, k);
    } else if (k == "DIV_MAX_N") {
      ret = Convert(v, Param::DIV_MAX_N, k);
    } else if (k == "PAR_THRES") {
      ret = Convert(v, Param::PAR_THRES, k);
    } else if (k == "NUM_THREADS") {
      ret = Convert(v, Param::NUM_THREADS, k);
    } else if (k == "MPC_BUF_SIZE") {
      ret = Convert(v, Param::MPC_BUF_SIZE, k);
    } else if (k == "ITER_PER_EVAL") {
      ret = Convert(v, Param::ITER_PER_EVAL, k);
    } else if (k == "NUM_DIM_TO_REMOVE") {
      ret = Convert(v, Param::NUM_DIM_TO_REMOVE, k);
    } else if (k == "NUM_OVERSAMPLE") {
      ret = Convert(v, Param::NUM_OVERSAMPLE, k);
    } else if (k == "NUM_POWER_ITER") {
      ret = Convert(v, Param::NUM_POWER_ITER, k);
    } else if (k == "PITER_BATCH_SIZE") {
      ret = Convert(v, Param::PITER_BATCH_SIZE, k);
    } else if (k == "OUTPUT_FILE_PREFIX") {
      ret = Convert(v, Param::OUTPUT_FILE_PREFIX, k);
    } else if (k == "LOG_FILE") {
      ret = Convert(v, Param::LOG_FILE, k);
    } else if (k == "CACHE_FILE_PREFIX") {
      ret = Convert(v, Param::CACHE_FILE_PREFIX, k);
      if (v[v.size() - 1] != '/') v += "/";
    } else if (k == "SNP_POS_FILE") {
      ret = Convert(v, Param::SNP_POS_FILE, k);
    } else if (k == "SKIP_QC") {
      ret = Convert(v, Param::SKIP_QC, k);
    } else if (k == "PROFILER") {
      ret = Convert(v, Param::PROFILER, k);
    } else if (k == "IMISS_UB") {
      ret = Convert(v, Param::IMISS_UB, k);
    } else if (k == "GMISS_UB") {
      ret = Convert(v, Param::GMISS_UB, k);
    } else if (k == "HET_LB") {
      ret = Convert(v, Param::HET_LB, k);
    } else if (k == "HET_UB") {
      ret = Convert(v, Param::HET_UB, k);
    } else if (k == "MAF_LB") {
      ret = Convert(v, Param::MAF_LB, k);
    } else if (k == "MAF_UB") {
      ret = Convert(v, Param::MAF_UB, k);
    } else if (k == "HWE_UB") {
      ret = Convert(v, Param::HWE_UB, k);
    } else if (k == "LD_DIST_THRES") {
      ret = Convert(v, Param::LD_DIST_THRES, k);
    } else if (k == "NUM_INDS") {
      ret = Convert(v, Param::NUM_INDS, k);
    } else if (k == "NUM_SNPS") {
      ret = Convert(v, Param::NUM_SNPS, k);
    } else if (k == "NUM_COVS") {
      ret = Convert(v, Param::NUM_COVS, k);
    } else if (k == "DEBUG") {
      ret = Convert(v, Param::DEBUG, k);
    } else {
      cout << "Unknown parameter: " << k << endl;
      ret = false;
    }

    if (!ret) {
      return false;
    }
  }

  return true;
}

int Param::PORT_P0_P1 = 8000;
int Param::PORT_P0_P2 = 8001;
int Param::PORT_P1_P2 = 8002;
int Param::PORT_P1_P3 = 8003;
int Param::PORT_P2_P3 = 8004;

string Param::IP_ADDR_P0 = "127.0.0.1";
string Param::IP_ADDR_P1 = "127.0.0.1";
string Param::IP_ADDR_P2 = "127.0.0.1";

string Param::KEY_PATH = "../stdlib/secure/key/";

int Param::NBIT_K = 60;
int Param::NBIT_F = 45;
int Param::NBIT_V = 64;

string Param::BASE_P = "101";
// string Param::BASE_P = "1461501637330902918203684832716283019655932542929";

uint64_t Param::MPC_BUF_SIZE = 1000000;

int Param::ITER_PER_EVAL= 5;
int Param::NUM_DIM_TO_REMOVE = 5;
int Param::NUM_OVERSAMPLE = 10;
int Param::NUM_POWER_ITER = 5;

string Param::OUTPUT_FILE_PREFIX = "../out/test";
string Param::CACHE_FILE_PREFIX = "../cache/test";
string Param::LOG_FILE = "../log/log.txt";
string Param::SNP_POS_FILE = "../test_data/pos.txt";

double Param::IMISS_UB = 0.05;
double Param::GMISS_UB = 0.1;
double Param::HET_LB = 0.25;
double Param::HET_UB = 0.30;
double Param::MAF_LB = 0.4;
double Param::MAF_UB = 0.6;
double Param::HWE_UB = 28.3740;
long Param::LD_DIST_THRES = 1000000;
long Param::DIV_MAX_N = 100000;

long Param::NUM_INDS = 1000;
long Param::NUM_SNPS = 1000;
long Param::NUM_COVS = 10;

long Param::PITER_BATCH_SIZE = 100;
long Param::PAR_THRES = 50;
long Param::NUM_THREADS = 20;

bool Param::SKIP_QC = false;
bool Param::PROFILER = true;
bool Param::DEBUG = false;
