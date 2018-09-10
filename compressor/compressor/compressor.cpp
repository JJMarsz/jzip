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
#include <set>
#include <ctime>

using namespace std;
#define BITSET_SIZE		20
#define WINDOW_SIZE		2
#define KILOBYTE		1024

float searchTime = 0.0;
float addHeaderTime = 0.0;

enum compressState {
	START,
	SEARCHING,
	VALID_REF
};

struct index_t {
	bool ref;
	int i;
	index_t() { i = 0; ref = false; }
};
struct node_t {
	shared_ptr<node_t> m_left;
	shared_ptr<node_t> m_right;
	int m_freq;
	char m_val;
	node_t(char word = ' ', int freq=0, shared_ptr<node_t> left = NULL, shared_ptr<node_t> right = NULL) :
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

void addHeader(int lastIndex, int currIndex, string& s, unordered_map<char, set<int>>& map) {
	const clock_t begin_time = clock();
	bitset<16> header((uint16_t)(currIndex - lastIndex));
	header.set(15);
	string hs;
	hs += (0xFF00 & header.to_ulong()) >> 8;
	hs += (0xFF & header.to_ulong());
	s = hs + s;
	
	for (auto it = map.begin(); it != map.end(); it++) {
		int count = it->second.size();;
		for (auto iter = it->second.begin(); count > 0;count--) {
			auto tempIter = iter;
			auto val = *iter;
			tempIter++;
			it->second.erase(*iter);
			iter = tempIter;
			it->second.insert(val+2);
		}
	}
	addHeaderTime += (float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void checkValid(set<int>& currIndices, set<int>& nextIndices) {
	const clock_t begin_time = clock();
	set<int> newIndices;
	for (auto it = nextIndices.begin(); it != nextIndices.end(); it++) {
		if (currIndices.find(*it - 1) != currIndices.end()) {
			//valid find, put into newIndices
			newIndices.insert(*it);
		}
	}
	currIndices = newIndices;
	searchTime += (float(clock() - begin_time) / CLOCKS_PER_SEC);
}

void getEncoded(shared_ptr<node_t>& root, unordered_map<char, string> &map, stack<bitset<1>> path) {
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

char getWord(int& index, string& encoded, shared_ptr<node_t>& root) {
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
	const clock_t begin_time = clock();
	if (argc != 3 || !(string(argv[1]) == "compress" || string(argv[1]) == "decompress")) {
		cout << "The format is: " << endl;
		cout << "./compressor compress/decompress (filename)" << endl;
		return 0;
	}
	//huffman
	//unordered_map<char, int> charMap;
	string line;
	string name(argv[2]);
	int start = string(argv[2]).length()-1;
	while (start > 0) {
		if (argv[2][start] == '.') {
			name = string(argv[2]).substr(0, start);
			break;
		}
		start--;
	}
	ifstream file(argv[2]);
	if (file.is_open()) {
		if (string(argv[1]) == "compress") {
			//charMap['\n'] = 0;
			//while (getline(file, line)) {
			//	for (int i = 0; i < line.length(); i++) {
			//		if(charMap.find(line[i]) == charMap.end())
			//			charMap[line[i]] = 0;
			//		charMap[line[i]]++;
			//	}
			//	if(!file.eof())
			//		charMap['\n']++;
			//}
			////put chars into a min heap to get an ordered list
			//priority_queue<shared_ptr<node_t>, vector<shared_ptr<node_t>>, comparison> pq;
			//for (auto it = charMap.begin(); it != charMap.end(); it++)
			//	pq.push(make_shared<node_t>(it->first, it->second));
			////create the huffman tree
			//shared_ptr<node_t> root;
			//while (pq.size() > 1) {
			//	shared_ptr<node_t> x = pq.top();
			//	pq.pop();
			//	shared_ptr<node_t> y = (pq.empty() ? NULL : pq.top());
			//	pq.pop();
			//	root = make_shared<node_t>(' ', x->m_freq + (y == NULL ? 0 : y->m_freq), x, y);
			//	pq.push(root);
			//}
			////search the tree and store the huffman encoding of each word into a map
			//unordered_map<char, string> encoded;
			//getEncoded(root, encoded, stack<bitset<1>>());
			//file.clear();
			//file.seekg(0, ios::beg);
			////create the bit string
			//string bitString = "";
			//while (getline(file, line)) {
			//	start = 0;
			//	for (int i = 0; i < line.length(); i++) {
			//		bitString += encoded[line[i]];
			//	}
			//	if (!file.eof())
			//		bitString += encoded['\n'];
			//}
			////store bitstring into unsigned chars and put into file
			//ofstream oFile(name + string(".jzip"), ios::out | ios::binary);
			////first store encoded map as a header
			//for (auto it = encoded.begin(); it != encoded.end(); it++) {
			//	stringstream ss(it->second);
			//	std::bitset<8> bits;
			//	ss.seekg(0, ss.end);
			//	int length = ss.tellg();
			//	ss.seekg(0, ss.beg);
			//	oFile << it->first;
			//	while (length - ss.tellg() > 8) {
			//		ss >> bits;
			//		char c = char(bits.to_ulong());
			//		oFile << c;
			//	}
			//	//now in case there is a left overe 0 < # of bits < 8
			//	if (length - ss.tellg() < 8) {
			//		//special case at end of bits where it doesnt allign in a char
			//		ss >> bits;
			//		int diff = 8 - (length - ss.tellg());
			//		bits = bits << diff;
			//		char c = char(bits.to_ulong());
			//		oFile << c;
			//	}
			//}
			//stringstream sstream(bitString);
			//sstream.seekg(0, sstream.end);
			//int length = sstream.tellg();
			//sstream.seekg(0, sstream.beg);
			//while (sstream.good()){
			//	std::bitset<8> bits;
			//	if (length - sstream.tellg() < 8) {
			//		//special case at end of bits where it doesnt allign in a char
			//		sstream >> bits;
			//		int diff = 8 - (length - sstream.tellg());
			//		bits = bits << diff;
			//	}
			//	else
			//		sstream >> bits;
			//	char c = char(bits.to_ulong());
			//	oFile << c;
			//}
			//cout << bitString.length() << endl;
			//oFile.close();

			//L277 compress
			/*
			Data encoding configurations:

			If bit flag is set to 0 indicating a reference

				  0    1 - 8    9 - 23
				-------------------------
				| 0 | Length | Distance |
				-------------------------

				0		: Flag
				1 - 8	: Length of the reference (3 to 258 bytes)
				9 - 23	: Distance backwards to the reference (1 to 32,768 bytes)

				This is only used if the raw data version of this is larger
				than the 24 bits needed to create the reference.

				Both length and distance use bits to represent the bytes of each
				respective field. Length has a constant 3 bytes added onto it
				because the value that is being refered to cannot be smaller
				than 3 bytes otherwise it wouldn't be compressing

			If bit flag is set to 1 to represent raw data

				  0    1 - 15	Length
				-------------------------
				| 0 | Length | Raw data |
				-------------------------

				0		: Flag
				1 - 15	: Length of the raw data (1 to 32,768 bytes)
				Length	: Length # of bytes of raw data

				Contains the raw data that will be used in a reference
			*/
			/*
			Psuedocode

			L277encode():
				state = START
				count = 0
				while(!file.eof()):
					c = byte of data

					switch(state):

					case START:
						if c in charMap:
							obtain set of indices from charMap
							state = SEARCHING
							count++
						else:
							move on to next byte

					case SEARCHING:
						if c in charMap:
							obtain set of indices from charMap
							if set of indices contains an index after one of the set of indices from the last byte:
								keep the valid indices that come after the current indices
								count++
								if count == 3:
									state = VALID_REFERENCE
							else:
								put c into charMap
								state = START
								count = 0
						else:
							put c into char map
							state = START
							count = 0;

					case VALID_REFERENCE:
						if c is in charMap:
							obtain set of indices from charMap
							if set of indices contains an index after one of the set of indices from the last byte:
								keep the valid indices that come after the current indices
							else:
								create the reference to the closest one and append to compressed string in memory
								mark this section of the input stream as compressed and non-referenceable
								state = START
						else:
							create the reference to the closest one and append to compressed string in memory
							mark this section of the input stream as compressed and non-referenceable
							put c into charMap
							state = START
			*/
			int byteCount = 0;
			int compressCount = 0;
			string output;
			//stores map from a char to a set of its relative indices that are all raw data
			unordered_map<char, set<int>> indexMap;
			int lastIndex = 0;
			set<int> validIndices;
			while (getline(file, line)) {
				compressState state = START;
				int lineLength = 0;
				int count = 0;
				//append new line character to line as getline ommits that
				if (!file.eof())
					line += '\n';
				for (int i = 0; i < line.length(); i++) {
					switch (state) {
					case START:
						if (indexMap.find(line[i]) == indexMap.end()) {
							indexMap[line[i]] = set<int>();
							indexMap[line[i]].insert(i + byteCount);
						}
						else {
							count++;
							state = SEARCHING;
							validIndices = indexMap[line[i]];
						}
						output += line[i];
						lineLength++;
						break;
					case SEARCHING:
						if (indexMap.find(line[i]) == indexMap.end()) {
							indexMap[line[i]] = set<int>();
							//insert chars into the map as these cannot be compressed
							for (int j = count; j > 0 ; j--) {
								indexMap[line[i - j]].insert(i - j + byteCount);
							}
							state = START;
							count = 0;
							i--;
						}
						else{
							checkValid(validIndices, indexMap[line[i]]);
							if (validIndices.size() > 0) {
								//atleast one index followed the previous set of indices
								count++;
								output += line[i];
								lineLength++;
								if (count >= 4)
									state = VALID_REF;
							}
							else {
								//insert chars into the map as these cannot be compressed
								for (int j = count; j > 0; j--) {
									indexMap[line[i - j]].insert(i - j + byteCount);
								}
								count = 0;
								state = START;
								i--;
							}
						}
						break;
					case VALID_REF:
						if (indexMap.find(line[i]) == indexMap.end()) {
							//create the pointer reference
							//first find closest index
							int closest = 40000;
							for (auto it = validIndices.begin(); it != validIndices.end(); it++) {
								if (*it < closest)
									closest = *it;
							}
							//reset the output string to not contain the uncompressed chars
							output = output.substr(0, output.length()-count);
							//now offset the index such that it points to the first character not the last;
							closest -= count;
							lineLength -= count;
							//flag should be 0, count offset by 3 to fit 3-258
							output += char(count-3);
							//dist offset by 1 to fit 1 - 32,768
							uint16_t dist = (byteCount + lineLength - closest) - 1;
							output += char((0xFF00 & dist) >> 8);
							output += char(0xFF & dist);

							//now that a reference is made, need to label previous data block as a set of raw bytes
							if(lastIndex != byteCount + lineLength)
								addHeader(lastIndex, byteCount + lineLength - 1, output, indexMap);

							//account for reference
							lineLength += 3;
							count = 0;
							state = START;
							i--;

							//account for raw bytes header
							lineLength += 2;
							lastIndex = byteCount + lineLength;

							compressCount++;
						}
						else {
							checkValid(validIndices, indexMap[line[i]]);
							if (validIndices.size() > 0 || count > 258) {
								//still possible to get more length into this reference
								count++;
								output += line[i];
								lineLength++;
							}
							else {
								//create the pointer reference
								//first find closest index
								int closest = 40000;
								for (auto it = validIndices.begin(); it != validIndices.end(); it++) {
									if (*it < closest)
										closest = *it;
								}
								//reset the output string to not contain the uncompressed chars
								output = output.substr(0, output.length() - count);
								//now offset the index such that it points to the first character not the last;
								closest -= count;
								lineLength -= count;
								//flag should be 0, count offset by 3 to fit 3-258
								output += char(count - 3);
								//dist offset by 1 to fit 1 - 32,768
								uint16_t dist = (byteCount + lineLength - closest) - 1;
								output += char((0xFF00 & dist) >> 8);
								output += char(0xFF & dist);

								//now that a reference is made, need to label previous data block as a set of raw bytes
								if (lastIndex != byteCount + lineLength)
									addHeader(lastIndex, byteCount + lineLength - 1, output, indexMap);

								//account for reference
								lineLength += 3;
								count = 0;
								state = START;
								i--;

								//account for raw bytes header
								lineLength += 2;
								lastIndex = byteCount + lineLength;

								compressCount++;
							}
						}
						break;
					}
				}
				//keep track of line length along with new line char
				byteCount += lineLength;
				//clean up indices if they are to far away
				for (auto it = indexMap.begin(); it != indexMap.end(); it++) {
					for (auto iter = it->second.begin(); iter != it->second.end();) {
						if (byteCount - *iter > WINDOW_SIZE*KILOBYTE) {
							auto tempIter = iter;
							tempIter++;
							it->second.erase(*iter);
							iter = tempIter;
						}
						else
							break;
					}
				}
			}
			ofstream ofile(name + string(".jzip"));
			ofile << output; 
			// do something
			cout << float(clock() - begin_time) / CLOCKS_PER_SEC << endl;
			cout << "Compressed: " << compressCount << endl;
			cout << "checkValid uses: " << searchTime << endl;
			cout << "addHeader uses: " << addHeaderTime << endl;
		}
		else {
			////test decompress
			////first convert from chars to a bit string
			//ifstream ifile(name + string(".jzip"), std::ifstream::ate | std::ios::binary);
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
			//ofstream ofile2(name + string("_decoded.txt"));
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

