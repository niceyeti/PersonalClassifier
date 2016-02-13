#ifndef HEADER_HPP
#define HEADER_HPP

#include <iostream>
#include <fstream>
#include <cctype>
//#include <sstream>
    #include <unistd.h>  //TODO: get rid of these, since they're are linux specific. calls are in matrix to file.
    #include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include <wait.h>
//#include <utility>
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set> //use these for result duplicate subkey filtering
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <cmath>

//defines max foreseeable line length in the freqTable.txt database
#define MAX_LINE_LEN 256
#define MAX_WORD_LEN 27 //determined by looking up on the internet. There are english words over 28 chars, but very uncommon.
#define NGRAM 5
#define MIN_MODEL_SIZE 100 //Minimum items sufficient to define an ngram model. This is arbitrary, for the sake of code error-checks.
#define READ_SZ 4095
#define BUFSIZE 4096
#define MAX_WORDS_PER_PHRASE 256  //these params are not very safe in updateNgramTable--possible segfaults
#define MAX_PHRASES_PER_READ 256
#define MAX_TOKENS_PER_READ 1024  //about bufsize / 4.
#define MAX_SENT_LEN 256  //not very robust. but a constraint is needed on the upperbound of sentence length, for phrase parsing data structures.
// avg sentence length is around 10-15 words, 20+ being a long sentence.
//#define PHRASE_DELIMITER '#'
//#define WORD_DELIMITER ' '
#define FILE_DELIMITER '|'
#define PERIOD_HOLDER '+'
#define ASCII_DELETE 127
#define INF_ENTROPY 9999  //constant for infinite entropy: 9999 bits is enormous (think of it as 2^9999) 
#define INF_PERPLEXITY 999999
#define NLAMBDASETS 8
#define NLAMBDAS 7
#define NGRAMS_IN_QSTR 1  //for knn, this is the number of words in the query string, which may be required as order-independent (eg, qstr word1+word2 == word2+word1)
#define NGRAMS_IN_POSTSTR 3
#define U32MAX 0xFFFFFFFF  // ~4 billion
#define U64MAX 0xffffffffffffffff  // ~a very large number
#define DBG 0
#define MAX_SUM_WORD_DIST 99999 //effectively infinity
//#define CODIST_DIAMETER 256
#define CODIST_DIAMETER 32
#define CONTEXT_RADIUS 15  //num previous context words for relating semantic distance
#define CLUSTER_RADIUS 15  //size of radius for estimating which cluster we're currently within
#define KMEANS 1000
#define MAX_RAND_MEANS 100
#define MATH_PI 3.14159265358979323846F
#define K_NEIGHBORS 100  //modify this parameter as an input to mergeDuplicates


//using namespace std;
using std::set;
using std::cout;
using std::getline;
using std::endl;
using std::string;
using std::vector;
using std::cin;
using std::map;
using std::unordered_map;
using std::multimap;
using std::unordered_set;
using std::list;
using std::sort;
using std::flush;
using std::pair;
using std::pow;
using std::ios;
using std::fstream;
using std::isnormal;

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long int U64;
typedef float FloatType;  //This makes it convenient to change the matrix native data type (float, double, long double) in a single place

#endif
