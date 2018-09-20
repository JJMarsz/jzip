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

#define DEBUG			1

#define BITSET_SIZE		20
#define BLOCK_SIZE		32
#define KB				1024
#define MAX_THREADS		4

mutex ifileLock;	//Protects the input file
ifstream ifile;		//The input file

mutex ofileLock;	//Protects the output file
ofstream ofile;		//The output file

mutex loaderLock;		//Protects the compressed block data ptr
char* dataptr = NULL;	//Maintains a ptr to the data for the loader thread

mutex threadLock;				//Protects the thread relevant info
condition_variable threadCV;	//Used to notify the main thread that a compression thread has completed
int threadCount = 0;			//Number of running compress/decompress threads
vector<thread> threadVec;		//Maintains the handles to every active thread

void getEncoded(node_t* root, unordered_map<char, string> &map, stack<bitset<1>> path) {
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

char getWord(int& index, string& encoded, node_t* root) {
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

	Bits: 0 - 15
	--------------------------------------------------------
	| Data Block Number (assigned at thread instantiation) | 0 - 65,535 
	--------------------------------------------------------

	16 - 31
	-------------------------------------------------
	| Data Block Length (Starting with Huffman Map) | 0 - 65,535
	-------------------------------------------------

	32 - ??

	-------------------------
	| 
	-------------------------

	This current setup allows for a max file size of 536MB

*/
void compressBlock(int blockNum, int threadNum) {
	//first read in memory into a buffer
	char * buff = new char[BLOCK_SIZE*KB];
	unique_lock<mutex> fk(ifileLock);
	fk.lock();
	ifile.seekg(BLOCK_SIZE*KB*blockNum);
	ifile.read(buff, BLOCK_SIZE*KB);
	fk.unlock();

	//create the node structure
	unordered_map<char, linkedList_t> indexMap;
	listNode_t* lastChar = NULL;
	//Creates an intricate linked structure of pointers pointing to previous bytes and previous occurences of a byte
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

	string output;
	listNode_t* sentinal = NULL;
	listNode_t* currNode = NULL;
	//index into the buffer starting at the end
	int i = BLOCK_SIZE * KB - 1;
	int lastIndex = i;
	//reverse iterate through the block.
	while (i > 0) {
		//find the current node
		sentinal = indexMap[buff[i]].find(i);
		currNode = sentinal->prevIndex;
		//find the longest same sequence of bytes
		int longestLength = 0;
		listNode_t* node = NULL;
		while (currNode != NULL) {
			//Find longest length 
			int currLength = compare(currNode, sentinal);
			//prefer the furthest away as it is guarenteed it is in this block
			if (currLength >= longestLength) {
				longestLength = currLength;
				node = currNode;
			}
			currNode = currNode->prevIndex;
		}
		//Has to atleast be 4 bytes such that is can be compressed into 3 bytes
		if (longestLength > 3) {
			//Although length can get over 258, it is rare and it doesn't matter as this will just compress the first 258 bytes
			int l = min(longestLength, 258);
			//label the pointed to nodes as crucial so that they dont get compressed
			{
				listNode_t* temp = node;
				for (int i = 0; i < l; i++) {
					temp->crucial = true;
					temp = temp->prevChar;
				}
			}
			//Before creating compressed node, create a uncompressed node from this index to the last node end
			if(i != lastIndex){
				string newData;
				makeBytes(false, lastIndex - i, newData, &buff[i + 1]);
				output = newData + output;
			}
			//create compressed node
			{
				string newData;
				makeBytes(true, l - 3, newData, NULL, i - node->index);
				output = newData + output;
			}
			//update lastIndex and i to be at the index before the compressed section
			i -= longestLength;
			lastIndex = i;
		}
		else {
			//account for this byte and continue searching
			i--;
		}

		//no need for the memory now
		sentinal = indexMap[buff[KB*BLOCK_SIZE-1]].find(KB*BLOCK_SIZE - 1);
		while (sentinal != NULL) {
			listNode_t* temp = sentinal->prevChar;
			sentinal->prevIndex = NULL;
			sentinal->prevChar = NULL;
			delete sentinal;
			sentinal = temp;
		}
		delete[] buff;

		//Verify the set of bytes is put into an uncompressed section
		{
			string newData;
			makeBytes(false, lastIndex + 1, newData, buff);
			output = newData + output;
		}
		//Now we have an output string consisting of the L277 compressed data

		//HUFFMAN
		//create a map to count the occurences of each byte
		unordered_map<char, int> charMap;
		for (int i = 0; i < output.length(); i++) {
			if(charMap.find(output[i]) == charMap.end())
				charMap[output[i]] = 0;
			charMap[output[i]]++;
		}
		//put chars into a min heap to get an ordered list
		priority_queue<node_t*, vector<node_t*>, comparison> pq;
		for (auto it = charMap.begin(); it != charMap.end(); it++)
			pq.push(new node_t(it->first, it->second));
		//create the huffman tree
		node_t* root;
		while (pq.size() > 1) {
			node_t* x = pq.top();
			pq.pop();
			node_t* y = (pq.empty() ? NULL : pq.top());
			pq.pop();
			root = new node_t(' ', x->m_freq + (y == NULL ? 0 : y->m_freq), x, y);
			pq.push(root);
		}
		//search the tree and store the huffman encoding of each word into a map
		unordered_map<char, string> encoded;
		getEncoded(root, encoded, stack<bitset<1>>());
		//create the bit string that will be written to file
		string bitString = "";
		for (int i = 0; i < output.length(); i++) {
			bitString += encoded[output[i]];
		}
		//no longer need the only l277 encoded string as bitString is huffman on l277 encode
		output.clear();
		//first store encoded map as a header
		/*
			Format is:

			  0 - 7
			----------
			| Length |
			----------
			  8 - 15
			----------
			|  Byte  |
			----------
			 16 - (16 + length)
			--------------------
			|     Encoding     |
			--------------------
		*/
		stringstream outputSS;
		for (auto it = encoded.begin(); it != encoded.end(); it++) {
			//take the arbitrarily lengthed encoding and put it into a string stream
			stringstream ss(it->second);
			std::bitset<8> bits;
			ss.seekg(0, ss.end);
			uint8_t length = ss.tellg();
			ss.seekg(0, ss.beg);
			outputSS << length;
			outputSS << it->first;
			while (length - ss.tellg() > 8) {
				ss >> bits;
				char c = char(bits.to_ulong());
				outputSS << c;
			}
			//now in case there is a left over 0 < # of bits < 8
			if (length - ss.tellg() < 8) {
				//special case at end of bits where it doesnt allign in a char
				ss >> bits;
				int diff = 8 - (length - ss.tellg());
				bits = bits << diff;
				char c = char(bits.to_ulong());
				outputSS << c;
			}
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
			outputSS << c;
		}
		cout << bitString.length() << endl;
	}
}

void fileLoader() {
	//store blocks in sequential order
	int blockIdx = 0;
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
	//Open a stream into the output and out of the input file
	ofile = ofstream(name + ".jzip");
	ifile = ifstream(argv[2]);
	if (ifile.is_open()) {
		if (string(argv[1]) == "compress") {
			//instantiate the file loader thread
			//MAKE ME
			ofile = ofstream(name + string(".jzip"), ios::out | ios::binary);

			//obtain max block num
			ifile.seekg(0, ifile.end);
			int length = ifile.tellg();
			ifile.seekg(0, ifile.beg);
			int maxBlocks = ((float)length) / ((float)BLOCK_SIZE);

			//obtain RAM info, primarily size of the stack
			//MAKE ME

			int blocksLaunched = 0;
			unique_lock<mutex> tk(threadLock);
			//create the thread vector with the threads
			tk.lock();
			for (int i = 0; i < min(blocksLaunched, MAX_THREADS); i++) {
				threadVec.push_back(thread(compressBlock, i, i));
				threadCount++;
				blocksLaunched++;
			}
			tk.unlock();

			//launch compression threads
			while (blocksLaunched < maxBlocks) {
				int threadIdx;
				//wait until notified that a thread at index has completed
				threadCV.wait(tk, [&](int index){ 
					threadIdx = index;
					return threadCount < MAX_THREADS; 
				});
				//grab the thread control block and put a new thread into it
				tk.lock();
				threadVec[threadIdx] = thread(compressBlock, blocksLaunched++, threadIdx);
				threadCount++;
				tk.unlock();
			}
			//all compression threads are officially launched and running
			//wait for the threads to complete
			for (auto& th : threadVec)
				th.join();
			//notify the file loader that the all the threads are done executing
			//so that is shuts down
			//MAKE ME

			//print out debug runtime informations
			if (DEBUG) {

			}

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
	//close down the output file and input file
	//all other threads should be shutdown so no locks are needed
	ifile.close();
	ofile.close();
    return 0;
}

