#include <fstream>
using namespace std;
#include <bitset>
#include <math.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
/*
class A {
   int b;
};


int main() {
  bitset<11> a("001011");
  cout << a << endl;
  bool tag = true;
  if (!tag == false) 
     cout << "hjaha " << endl;
 int s = 0;
  while (1) {
  	char y;
  	cin >> y;
  	s = generate(s);
  }
}
*/

int main() {

	string file_name = "stategy.txt";
	ifstream infile(file_name.c_str(),ios::in);

	vector<int> v;
	int pos = 3;
	int index = 0;
	int asize = 0;
	infile >> asize;

	cout << asize<< endl;;
	for (int i = 0; i < asize; ++i) {
		for (int j = 0; j < 4; ++j) {
			int s;
			if (j == pos) {				
				infile >> s;
				v.push_back(s);
			}
			else {
				infile >> s;
			}
		}
	} 

	for (int i = 0; i < v.size(); ++i) {
		cout << v[i] << endl;
	}
}