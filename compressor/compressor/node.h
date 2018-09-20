#ifndef NODE_H
#define NODE_H

#include <memory>
/*
	Represnts a byte in memory with pointers to the previous byte and
	previous occurence of this same byte
*/
struct listNode_t {
	listNode_t* prevChar;
	listNode_t* prevIndex;
	uint16_t index;
	char byte;
	bool crucial;
	listNode_t(int i, char ch) :
		index(i),
		byte(ch) {
		crucial = false;
	}
};
/*
	Maintains a linked list of same character nodes
*/
class linkedList_t {
public:
	linkedList_t(listNode_t* h = NULL, listNode_t* t = NULL, char cc = 0x00) :
		head(h),
		tail(t),
		c(cc) {}
	~linkedList_t() {
		listNode_t* temp = head;
		while (head != NULL) {
			temp = head->prevIndex;
			delete head;
			head = temp;
		}
	}
	//gets most recent addition
	listNode_t* get() { return head; }
	listNode_t* find(int i) {
		listNode_t* sentinal = tail;
		while (sentinal != NULL) {
			if (sentinal->index == i)
				break;
			sentinal = sentinal->prevIndex;
		}
		return sentinal;
	}
	//inserts a node at the end of the list
	void insert(int i) {
		//empty list
		listNode_t* node = new listNode_t(i, c);
		if (tail == NULL) {
			tail = node;
			head = node;
		}
		//atleast one node
		else {
			node->prevIndex = head;
			head = node;
		}
	}
private:
	listNode_t* head;
	listNode_t* tail;
	char c;
};
struct node_t {
	shared_ptr<node_t> m_left;
	shared_ptr<node_t> m_right;
	int m_freq;
	char m_val;
	node_t(char word = ' ', int freq = 0, shared_ptr<node_t> left = NULL, shared_ptr<node_t> right = NULL) :
		m_freq(freq),
		m_val(word),
		m_left(left),
		m_right(right) {}
};
/*
	Returns the length of similarity between curr and test
*/
int compare(listNode_t* test, listNode_t* curr) {
	int count = 0;
	while (test != NULL && curr != NULL) {
		//Make sure the previous char is the same and the current run is not a crucial byte
		if (curr->byte == test->byte && !curr->crucial) {
			count++;
			test = test->prevChar;
			curr = curr->prevChar;
		}
		else
			break;
	}
	return count;
}

/*
	Creates an L277 compressed data section

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

void makeBytes(bool compressed, int length, string& odata, char* idata = NULL, int dist=0) {
	//make the data bytes
	if (compressed) {
		//make 3 byte compression refernece
		//guarentee first bit is 0 as per required, 
		odata += char((uint8_t)(length) >> 1);
		//get first 7 bits of distance along with last bit of length
		char byte2 = (uint8_t)((0xEF00 & dist) >> 8);
		//first bit of byte 2 is just the even/odd of length
		byte2 |= (length % 2 ? 0x80 : 0x00);
		odata += byte2;
		//last 8 bits of distance
		odata += char(0xFF & dist);
	}
	else {
		if (idata != NULL) {
			for (int i = 0; i < length; i++)
				odata += idata[i];
		}
		//make 2 byte length byte + data
		//second byte is last 8 bits of length
		odata = char((uint16_t)(0xFF & length)) + odata;
		//first byte is 1 + first 7 bits of length
		odata = char(0x80 | ((uint16_t)(0xEF00 & length) >> 8)) + odata;
	}
}
#endif 