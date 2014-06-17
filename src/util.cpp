#include "util.h"

using namespace std;

int strtoint(const string& s) {
	size_t l;
	int i = stoi(s, &l, 0);
	if (l != s.size()) throw logic_error("error parsing int: " + s);
	return i;
}

float strtofloat(const string& s) {
	size_t l;
	float f = stof(s, &l);
	if (l != s.size()) throw logic_error("error parsing float: " + s);
	return f;
}

