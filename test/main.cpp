#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <dirent.h>
#include <seq/seq.h>
#include <seq/parser.h>

using namespace seq;
using namespace std;

vector<string> splitlines(const string &output)
{
	vector<string> result;
	string line;
	istringstream stream(output);
	const char delim = '\n';

	while (getline(stream, line, delim))
		result.push_back(line);

	return result;
}

static vector<string> findExpects(const string& filename)
{
	ifstream file(filename);

	if (!file.good()) {
		cerr << "error: could not open " << filename << endl;
		exit(EXIT_FAILURE);
	}

	string line;
	regex expect(R"(#\s*EXPECT\s*:\s*([^\n]*))");
	smatch m;
	vector<string> result;

	while (getline(file, line)) {
		if (regex_search(line, m, expect))
			result.push_back(m[1]);
	}

	file.close();
	return result;
}

static bool runTest(const string& filename, bool debug)
{
	vector<string> expects = findExpects(filename);
	stringstream buffer;
	streambuf *old = cout.rdbuf(buffer.rdbuf());
	SeqModule *module = parse(filename);
	execute(module, {}, {}, debug);
	cout.rdbuf(old);
	vector<string> results = splitlines(buffer.str());

	bool pass = true;

	cout << "TEST: " << filename << endl;
	if (results.size() != expects.size()) {
		cout << "  GOT:" << endl;
		for (auto& line : results)
			cout << "    " << line << endl;

		cout << "  EXP:" << endl;
		for (auto& line : expects)
			cout << "    " << line << endl;

		cout << "FAIL: output size mismatch" << endl;
		pass = false;
	} else {
		for (unsigned i = 0; i < results.size(); i++) {
			bool casePass = (results[i] == expects[i]);
			cout << "  Case " << (i + 1) << ": " << (casePass ? "PASS" : "FAIL") << endl;
			cout << "    GOT: " << results[i] << endl;
			cout << "    EXP: " << expects[i] << endl;

			if (!casePass)
				pass = false;
		}

		if (!pass)
			cout << "FAIL: output mismatch" << endl;
	}

	if (pass)
		cout << "PASS" << endl;
	cout << endl;

	return pass;
}

static bool isSeqFile(const std::string& filename) {
	static const string ext = ".seq";
	if (filename.length() >= ext.length())
		return (filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0);
    else
		return false;
}

static bool runTestsFromDir(const std::string& path, bool debug)
{
	DIR *dir;
	struct dirent *ent;
	bool pass = true;

	if ((dir = opendir(path.c_str()))) {
		while ((ent = readdir(dir))) {
			if (isSeqFile(string(ent->d_name)) &&
			    !runTest(path + "/" + ent->d_name, debug)) {
				pass = false;
			}
		}

		closedir(dir);
	} else {
		cerr << "error: could not open " << path << endl;
		exit(EXIT_FAILURE);
	}

	return pass;
}

int main(int argc, char *argv[])
{
	bool pass = runTestsFromDir(TEST_DIR "/core", false);
	return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
