#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set> //use these for result duplicate subkey filtering
#include <vector>
#include <iostream>
#include <fstream>
#include <cctype>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <wait.h>
#include <utility>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <set>
#include <cmath>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/resource.h>


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
#define U64MAX 0xffffffffffffffff  // ~a very large numbr
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

typedef map<U32,FloatType> RowMap;  //each row in the matrix is actually a map, saving significant space, since many matrix entries are empty, and thus not stored in a map
typedef map<U32,RowMap> CoDistanceTable;  //these were unordered_map's, which were faster, at little extra mem size
typedef CoDistanceTable::iterator CoDistOuterIt;
typedef RowMap::iterator CoDistInnerIt;
typedef unordered_map<string,pair<U32,vector<U32> > > IndexTable;
typedef IndexTable::iterator IndexTableIt;
typedef unordered_map<U32,string> IdTable;  //this complicates things massively, but reduces mem consumption
typedef IdTable::iterator IdTableIt;
typedef map<string,U16> ClusterMap;
typedef ClusterMap::iterator ClusterMapIt;
typedef map<U16,string> ClusterIdMap;
typedef ClusterIdMap::iterator ClusterIdMapIt;
typedef U32 wordKey;


class kNN{
  private:

    //encapsulated word-table class: maps words (strings) to integer ids and vice versa, so we only need store integer-ids
    //as indices in various data structures, rather than storing the whole strings.
    WordIdManager m_wordIdManager;

    CoDistanceTable CoDistTable; //expensive to build, but not to store. Best to preprocess this based on research
    IndexTable wordIndexTable;  //an intermediate table for calculating distances and other utilities (k-means)
    //a nuisance, but this is intended to reduce memory size. its just a hack for creating a middleman
    //between strings (many chars) and U32 ids, such that the tables only need to store U32 size keys.
    IdTable wordIdTable;


    unordered_set<string> stopWords;
    bool hasColon(const char buf[], int len);
    int findChar(const char buf[], char c, int len);
    void scrubHyphens(string& istr);
    bool isPhraseDelimiter(char c);
    bool isWordDelimiter(char c);
    void toLower(string& myStr);
    void toLower(char buf[BUFSIZE]);
    void delimitText(string& istr);
    int seekLastPhraseDelimiter(const char buf[BUFSIZE], int len);
    void rawPass(string& istr);
    void finalPass(string& buf);
    void normalizeText(char ibuf[BUFSIZE], string& obuf);
    int tokenize(char* ptrs[], char buf[BUFSIZE], const string& delims);
    bool isDelimiter(const char c, const string& delims);
    bool isValidWordExtended(const string& token); //new as of kNN
    bool isValidWordExtended(const char* word); //overload of above foo()
    bool isValidWord(const string& token); //new as of kNN
    bool isValidWord(const char* word); //overload of above foo()
    bool isInt(const string& nstr);
    void initValidDecimalChars(void);
    bool isDecimal(const string& nstr);
    timespec DiffTimeSpecs(timespec* start, timespec* end);

    //word similarity measures
    FloatType method_Poisson1(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    FloatType method_Poisson2(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    FloatType method_LogLikelihood(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    //FloatType buildSimilarityTable(CoDistanceTable& coOccurrenceTable, CoDistanceTable& similarityTable);
    void buildSimilarityTable(CoDistanceTable& similarityTable);
    FloatType method_binaryCosine(RowMap& row1, RowMap& row2);
    FloatType method_weightedCosine(RowMap& row1, RowMap& row2);

    //utils
    void pruneWordSequence(const vector<string>& inVec, vector<string>& outVec);
    char* getLine(int openfd, char buf[], int len);
    bool isStopWord(const string& word);
    bool isStopWord(const char* word);
    void buildStopWordTable(const string& stopWordFile);
    void initWordIdTable(void);
    void textToWordSequence(const string& trainingFile, vector<string>& wordSequence, bool filterStopWords);
    U32 buildWordIndexTable(const vector<string>& wordSequence);
    void validateWordKeyTables(void);
    void clearWordIndexTable(void);
    U32 allocId(const string& word);
    U32 allocId(const string& word, IndexTableIt it);
    string idToStr(const U32 id);
    U32 strToId(const string& word);

    void printTopWords(int n);
    void copyVecs(const vector<string>& src, vector<string>& dest);
    void pruneWordIndexTable(IndexTable& wordTable);
    bool ShareCoOccurrences(CoDistanceTable& simTable, RowMap& row, U32 key1, U32 key2);
    bool CoDistTableHasKey(CoDistanceTable& coDistTable, const string& k1, const string& k2, CoDistInnerIt& it);
    bool CoDistTableHasKey(CoDistanceTable& coDistTable, const U32 key1, const U32 key2, CoDistInnerIt& it);
    void updateCoDistTable(const string& key1, const string& key2);
    void coDistTableToFile(string& fname);
    FloatType getSumDistance(const string& w1, const string& targets);
    FloatType getClusterDistance(U16 clusterNo, const string& postStr);
    //U32 getSimilarity(const string& w1, const string& w2); //old static version: returned nAB
    FloatType getSimilarity(const string& w1, const string& w2);
    //double getSumSimilarity(const U32 w1);
    FloatType getSumSimilarity(const string& w1);
    FloatType calculateSignificance(const U32 w1, const U32 w2);
    void buildCoDistTable(vector<string>& wordSequence);
    void buildCoDistTableFromFile(const string& fname);

  public:
    kNN();
    ~kNN();
    void buildCoDistTable(vector<string>& wordSequence);
    void buildCoDistTableFromFile(const string& fname);
};





























