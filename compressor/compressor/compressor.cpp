// compressor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <memory>
#include <bitset>
#include <stack>
#include <sstream>

using namespace std;
#define BITSET_SIZE		20
struct node_t {
	shared_ptr<node_t> m_left;
	shared_ptr<node_t> m_right;
	int m_freq;
	string m_val;
	node_t(string word = "", int freq=0, shared_ptr<node_t> left = NULL, shared_ptr<node_t> right = NULL) :
	m_freq(freq),
	m_val(word),
	m_left(left),
	m_right(right)
	{}
};

class comparison {
public:
	bool operator() (const shared_ptr<node_t>& lhs, const shared_ptr<node_t>&rhs) const {
		return (lhs->m_freq > rhs->m_freq);
	}
};

void getEncoded(shared_ptr<node_t>& root, unordered_map<string, string> &map, stack<bitset<1>> path) {
	if (root->m_left == NULL && root->m_right == NULL) {
		stack<bitset<1>> temp;
		string val;
		while (!path.empty()) {
			val = path.top().to_string() + val;
			path.pop();
		}
		map[root->m_val] = val;
		return;
	}
	path.push(bitset<1>(0));
	getEncoded(root->m_left, map, path);
	path.pop();
	path.push(bitset<1>(1));
	getEncoded(root->m_right, map, path);
	path.pop();
}

string getWord(int& index, string& encoded, shared_ptr<node_t>& root) {
	if(root->m_left == NULL && root->m_right == NULL) {
		//found word
		return root->m_val;
	}
	index++;
	if (encoded[index-1] == '1')
		return getWord(index, encoded, root->m_right);
	else
		return getWord(index, encoded, root->m_left);

}

int main(int argc, char *argv[]){
	if (argc != 3 || !(string(argv[1]) == "compress" || string(argv[1]) == "decompress")) {
		cout << "The format is: " << endl;
		cout << "./compressor compress/decompress (filename)" << endl;
		return 0;
	}
	//huffman
	unordered_map<string, int> wordMap;
	string line;
	int start = 0;
	ifstream file(argv[2]);
	if (file.is_open()) {
		if (string(argv[1]) == "compress") {
			wordMap["\n"] = 0;
			while (getline(file, line)) {
				string word;
				start = 0;
				for (int i = 0; i < line.length(); i++) {
					if (line[i] == ' ' || i == line.length()-1) {
						if (i == line.length() - 1) {
							word = line.substr(start, i - start + 1);
							wordMap["\n"]++;
						}
						else
							word = line.substr(start, i - start);
						if (wordMap.find(word) == wordMap.end()) {
							wordMap[word] = 0;
						}
						wordMap[word]++;
						start = i + 1;
					}
				}
				if(line.length() == 0)
					wordMap["\n"]++;
			}
			if (wordMap.size() < 2) {
				cout << "Why are you trying to compress a file with one unique word..." << endl;
				file.close();
				return 0;
			}
			//put words into a min heap to get an ordered list
			priority_queue<shared_ptr<node_t>, vector<shared_ptr<node_t>>, comparison> pq;
			for (auto it = wordMap.begin(); it != wordMap.end(); it++)
				pq.push(make_shared<node_t>(it->first, it->second));
			//create the huffman tree
			int val = 0;
			shared_ptr<node_t> root;
			while (pq.size() > 1) {
				shared_ptr<node_t> x = pq.top();
				pq.pop();
				shared_ptr<node_t> y = (pq.empty() ? NULL : pq.top());
				pq.pop();
				root = make_shared<node_t>("", x->m_freq + (y == NULL ? 0 : y->m_freq), x, y);
				pq.push(root);
			}
			//search the tree and store the huffman encoding of each word into a map
			unordered_map<string, string> encoded;
			getEncoded(root, encoded, stack<bitset<1>>());
			file.clear();
			file.seekg(0, ios::beg);
			//create the bit string
			string bitString = "";
			while (getline(file, line)) {
				start = 0;
				for (int i = 0; i < line.length(); i++) {
					if (line[i] == ' ') {
						bitString += encoded[line.substr(start, i - start)];
						start = i + 1;
					}
					if (i == line.length() - 1) {
						bitString += encoded[line.substr(start, i - start + 1)];
						bitString += encoded["\n"];
					}
				}
				if (line.length() == 0)
					bitString += encoded["\n"];
			}
			//store bitstring into unsigned chars and put into file
			ofstream oFile(argv[2] + string(".jzip"), ios::out | ios::binary);
			//first store encoded map as a header
			for (auto it = encoded.begin(); it != encoded.end(); it++) {
				stringstream ss(it->second);
				std::bitset<8> bits;
				ss.seekg(0, ss.end);
				int length = ss.tellg();
				ss.seekg(0, ss.beg);
				oFile << it->first;
				while (length - ss.tellg() > 8) {
					ss >> bits;
					char c = char(bits.to_ulong());
					oFile << c;
				}
				//now in case there is a left overe 0 < # of bits < 8
				if (length - ss.tellg() < 8) {
					//special case at end of bits where it doesnt allign in a char
					ss >> bits;
					int diff = 8 - (length - ss.tellg());
					bits = bits << diff;
					char c = char(bits.to_ulong());
					oFile << c;
				}
				//delimiter?

			}
			stringstream sstream(bitString);
			sstream.seekg(0, sstream.end);
			int length = sstream.tellg();
			sstream.seekg(0, sstream.beg);
			while (sstream.good()){
				std::bitset<8> bits;
				if (length - sstream.tellg() < 8) {
					//special case at end of bits where it doesnt allign in a char
					sstream >> bits;
					int diff = 8 - (length - sstream.tellg());
					bits = bits << diff;
				}
				else
					sstream >> bits;
				char c = char(bits.to_ulong());
				oFile << c;
			}
			oFile.close();
		}
		else {
			////test decompress
			////first convert from chars to a bit string
			//ifstream ifile(argv[2] + string(".jzip"), std::ifstream::ate | std::ios::binary);
			//int size = ifile.tellg();
			//ifile.clear();
			//ifile.seekg(0, ios::beg);
			//vector<char> buffer(size);
			//ifile.read(&buffer[0], size);
			//string bitString = "";
			//for (int i = 0; i < buffer.size(); i++) {
			//	std::bitset<8> bits(buffer[i]);
			//	bitString += bits.to_string();
			//}
			//ifile.close();
			////take converted bitstring and decode it
			//ofstream ofile2(argv[2] + string("decoded.txt"));
			//int index = 0;
			//bool sol = true;
			//string decoded;
			////bitString = bitString.substr(0, bitString.length() - diff);
			////cout << bitString.substr(224440) << " :BitString" << endl;
			//while (index < bitString.length() - 8) {
			//	decoded = getWord(index, bitString, root);
			//	if (decoded != "\n" && !sol) {
			//		ofile2 << " ";
			//	}
			//	ofile2 << decoded;
			//	if (decoded == "\n")
			//		sol = true;
			//	else
			//		sol = false;
			//}
			//ofile2.close();
		}
	}
	else
		cout << "Cannot open file" << endl;
	file.close();
    return 0;
}

