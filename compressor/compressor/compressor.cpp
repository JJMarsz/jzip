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
#include <mutex>
#include "node.h"

using namespace std;
#define BITSET_SIZE		20
#define BLOCK_SIZE		32
#define KB				1024
#define MAX_THREADS		4

condition_variable threadCV;	//Used to notify the main thread that a compression thread has completed

mutex ifileLock;	//Protects the input file
mutex ofileLock;	//Protects the output file
mutex loaderLock;	//Protects the compressed block data ptr
mutex threadLock;	//Protects the thread conditional variable 
mutex mainLock;		//Protects the main thread from being notified when not waiting

int threadCount = 0;
char* dataptr;

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
/*
  Takes in loaded memory and compresses it using L277 and Huffman

	First it creates a linked list where each byte is a node
	with two pointers. the prevChar pointer points to the
	byte that preceeds this char in memory. The prevIndex 
	points to the last node that had the same byte. Once this
	is created, the buffer memory is no longer needed and
	can be freed.

	Then it starts with the last byte and backward traverses
	the linked list. At every byte, check every previous instance
	of this byte for the longest repeated byte string with
	the current byte as the start. 
	If this is longer than 3 bytes, it is worth making a pointer. 
		Create an uncompressed node consisting of the data starting 
		after the current byte to the lastIndex, which represents the 
		end of the uncompressed data run. Free the the nodes in the 
		original linked list that this dataNode represents. Prepend this 
		data to the dataString. Create a compressed node consisting 
		of the length of of data being repeated and the index offset
		to this repetition. Free the nodes that represent this data
		and prepend this to the dataString
	else
		move on to the next byte and try again

	Once all the data has been looked at, verify the linked list
	is completely freed, otherwise make an uncompressed node with
	the remaining data, prepend it to the dataString, and free the 
	rest of the linked list.

	Now we have a byte string of this L277 encoded dataset. Create a 
	map of byte value to its count. 

	Push each mapping into a priority queue as a pair with a custom
	comparator for figuring out the order. This is a min heap.

	Perform the Huffman Tree construction algorithm. Pop the two
	smallest values. Create a new parent node with the two smallest
	as children. This parent node's val is the cumulative sum of the
	two child nodes. Push the parent node into the priority queue.

	Once the priority queue reaches a size of one, we have a successful
	Huffman Tree.

	Reading the dataString constructed in the L277 encoding section,
	replace bytes with their huffman encoded equivalent. Using a
	vector of bitset<8>, append the bits in correctly formatted bytes
	to the vector.

	Once all are appended, we have all the compressed data and huffman
	tree and block data needed to construct a data block. The block
	structure is as follows:

	0 - 15
	--------------------------------------------------------
	| Data Block Number (assigned at thread instantiation) |
	--------------------------------------------------------

	16 - 31
	-------------------------------------------------
	| Data Block Length (Starting with Huffman Map) |
	-------------------------------------------------

	32 - ??

	-------------------------
	| 
	------------------------


*/
void compressBlock(ifstream file, int length, int blockNum) {
	//first read in memory into a buffer
	char * buff = new char[BLOCK_SIZE*KB];
	unique_lock<mutex> fk(ifileLock);
	fk.lock();
	file.seekg(BLOCK_SIZE*KB*blockNum);
	file.read(buff, BLOCK_SIZE*KB);
	fk.unlock();

	//create the node structure
	string output;
	unordered_map<char, linkedList_t> indexMap;
	listNode_t* lastChar = NULL;
	for (int i = 0; i < BLOCK_SIZE*KB; i++) {
		if (indexMap.find(buff[i]) == indexMap.end()) {
			indexMap[buff[i]] = linkedList_t(NULL, NULL, buff[i]);
		}
		indexMap[buff[i]].insert(i);
		if (lastChar != NULL) {
			indexMap[buff[i]].get()->prevChar = lastChar;
		}
		lastChar = indexMap[buff[i]].get();
	}
	//no need for the memory now
	delete[] buff;

	dataNode_t* dataNode = NULL;
	listNode_t* sentinal = NULL;
	listNode_t* currNode = NULL;
	compressState state = START;
	//index into the buffer
	int i = BLOCK_SIZE * KB - 1;
	int lastIndex = i;
	//reverse iterate through the block.
	while (sentinal != NULL) {
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
			//update lastIndex
			lastIndex = i + 1;
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



		//HUFFMAN


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
	}
}

int main(int argc, char *argv[]){
	//const clock_t begin_time = clock();
	if (argc != 3 || !(string(argv[1]) == "compress" || string(argv[1]) == "decompress")) {
		cout << "The format is: " << endl;
		cout << "./compressor compress/decompress (filename)" << endl;
		return 0;
	}
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
			//instantiate the file writer thread
			//MAKE ME

			//obtain max block num
			file.seekg(0, file.end);
			int length = file.tellg();
			file.seekg(0, file.beg);
			int maxBlocks = ((float)length) / ((float)BLOCK_SIZE);

			//obtain RAM info, primarily size of the stack
			//MAKE ME
			int blocksDone = 0;
			//launch compression threads
			do {
				//launch as many threads as possible
				for (int i = 0; i < 4 - threadCount; i++) {

				}

				//wait until notified that a thread has completed
				unique_lock<mutex> tk(threadLock);
				threadCV.wait(tk, [] {return threadCount < MAX_THREADS; });
			} while (blocksDone <= maxBlocks);
			
			/*ofstream ofile(name + string(".jzip"));
			ofile << output;*/
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

