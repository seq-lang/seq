#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <seq/seq.h>
#include <seq/parser.h>

using namespace seq;
using namespace std;

template <size_t N>
struct CaptureStdout {
	char buf[N];
	int out_pipe[2];
	int save;

	CaptureStdout() :
	    buf(), out_pipe(), save()
	{
		memset(buf, '\0', N);
		save = dup(STDOUT_FILENO);
		assert(pipe(out_pipe) == 0);
		long flags = fcntl(out_pipe[0], F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(out_pipe[0], F_SETFL, flags);
		dup2(out_pipe[1], STDOUT_FILENO);
		close(out_pipe[1]);
	}

	~CaptureStdout()
	{
		dup2(save, STDOUT_FILENO);
	}

	string result()
	{
		fflush(stdout);
		read(out_pipe[0], buf, sizeof(buf) - 1);
		return string(buf);
	}
};

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

static string findExpectOnLine(const string& line)
{
	static const string EXPECT_STR = "# EXPECT: ";
	size_t pos = line.find(EXPECT_STR);
	return pos == string::npos ? "" : line.substr(pos + EXPECT_STR.length());
}

static vector<string> findExpects(const string& filename)
{
	ifstream file(filename);

	if (!file.good()) {
		cerr << "error: could not open " << filename << endl;
		exit(EXIT_FAILURE);
	}

	string line;
	vector<string> result;

	while (getline(file, line)) {
		string expect = findExpectOnLine(line);
		if (!expect.empty())
			result.push_back(expect);
	}

	file.close();
	return result;
}

static bool runTest(const string& filename, bool debug)
{
	cout << "TEST: " << filename << endl;
	vector<string> expects = findExpects(filename);
	vector<string> results;

	{
		CaptureStdout<10000> capture;
		SeqModule *module = parse(filename);
		execute(module, {}, {}, debug);
		results = splitlines(capture.result());
	}

	bool pass = true;
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

static bool isSeqFile(const string& filename)
{
	static const string ext = ".seq";
	if (filename.length() >= ext.length())
		return (filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0);
	else
		return false;
}

static bool runTestsFromDir(const string& path, bool debug)
{
	DIR *dir;
	struct dirent *ent;
	bool pass = true;
	vector<string> filesToRun;

	if ((dir = opendir(path.c_str()))) {
		while ((ent = readdir(dir))) {
			if (isSeqFile(string(ent->d_name)))
				filesToRun.push_back(path + "/" + ent->d_name);
		}

		closedir(dir);
		sort(filesToRun.begin(), filesToRun.end());

		for (auto& file : filesToRun) {
			if (!runTest(file, debug))
				pass = false;
		}
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
