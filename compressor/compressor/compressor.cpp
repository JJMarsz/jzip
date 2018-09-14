// compressor.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <bitset>
#include <stack>
#include <sstream>
#include <set>
#include <ctime>
#include "node.h"

using namespace std;
#define BITSET_SIZE		20
#define BLOCK_SIZE		32
#define KB				1024

float searchTime = 0.0;
float addHeaderTime = 0.0;

enum compressState {
	START,
	SEARCHING,
	VALID_REF
};

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

			//first read in memory into a buffer
			char * buff = new char[BLOCK_SIZE*KB];
			file.read(buff, BLOCK_SIZE*KB);
			//create the node structure
			string output;
			unordered_map<char, linkedList_t> indexMap;
			listNode_t* lastChar = NULL;
			for (int i = 0; i < BLOCK_SIZE*KB;i++) {
				if (indexMap.find(buff[i]) == indexMap.end()) {
					indexMap[buff[i]] = linkedList_t(NULL, NULL, buff[i]);
				}
				indexMap[buff[i]].insert(i);
				if (lastChar != NULL) {
					indexMap[buff[i]].get()->prevChar = lastChar;
				}
				lastChar = indexMap[buff[i]].get();
			}

			int lastIndex = KB * BLOCK_SIZE - 1;
			dataNode_t* dataNode = NULL;
			listNode_t* sentinal = NULL;
			listNode_t* currNode = NULL;
			compressState state = START;
			//index into the buffer
			int i = BLOCK_SIZE*KB;
			//reverse iterate through the block.
			while(sentinal != NULL) {
				//find the current node
				sentinal = indexMap[buff[i]].find(i);
				currNode = sentinal->prevIndex;
				//find the longest same sequence of bytes
				int longestLength = 0;
				listNode_t* node = NULL;
				while (sentinal != NULL) {
					//Find longest length 
					int currLength = compare(sentinal, currNode);
					//prefer the furthest away as it is guarenteed it is in this block
					if (currLength >= longestLength) {
						longestLength = currLength;
						node = currNode;
					}
					currNode = currNode->prevIndex;
				}
				if (longestLength > 3) {
					//Before creating compressed node, create a uncompressed node from this index to the last node end
					dataNode_t* raw = new dataNode_t(false, lastIndex - i, 0, &buff[i]);
					if (dataNode == NULL)
						dataNode = raw;
					else {
						//put the node into the data list
						raw->nextNode = dataNode;
						dataNode = raw;
					}
					//create compressed node
					int l = min(longestLength, 258);
					dataNode_t* compressed = new dataNode_t(true, l - 3, sentinal->index - node->index - 1);
					//put the node into the data list
					compressed->nextNode = dataNode;
					dataNode = compressed;
					i -= l;
					//
				}
				else {
					//account for this byte and continue searching
					i--;
				}
			}

			delete[] buff;
			ofstream ofile(name + string(".jzip"));
			ofile << output; 
			// do something
			cout << float(clock() - begin_time) / CLOCKS_PER_SEC << endl;
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

