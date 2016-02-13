#ifndef HEADER_HPP
  #include "Header.hpp"
#endif

#ifndef TEXTPROCESSOR_HPP
  #include "TextProcessor.hpp"
#endif

#ifndef WORDIDMANAGER_HPP
  #include "WordIdManager.hpp"
#endif




typedef map<U32,FloatType> RowMap;  //each row in the matrix is actually a map, saving significant space, since many matrix entries are empty, and thus not stored in a map
typedef map<U32,RowMap> Matrix;  //these were unordered_map's, which were faster, at little extra mem size
typedef Matrix::iterator MatrixOuterIt;
typedef RowMap::iterator MatrixInnerIt;
typedef Matrix::const_iterator ConstMatrixOuterIt;
typedef RowMap::const_iterator ConstMatrixInnerIt;
typedef map<string,vector<U32> > WordIndexTable;
typedef WordIndexTable::iterator WordIndexTableIt;
typedef pair<U32,double> ResultantEntry;
typedef vector<ResultantEntry > DotProductVector;

class PersonalClassifier{
  private:

    //encapsulated word-table class: maps words (strings) to integer ids and vice versa, so we only need store integer-ids
    //as indices in various data structures, rather than storing the whole strings.
    WordIdManager* m_wordIdManager;

    //encapsulated text processing class, just provides text processing services of various kinds
    TextProcessor* m_textProcessor;

/*
    //word similarity measures
    FloatType method_Poisson1(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    FloatType method_Poisson2(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    FloatType method_LogLikelihood(FloatType n, FloatType nA, FloatType nB, FloatType nAB);
    //FloatType buildSimilarityTable(Matrix& coOccurrenceTable, Matrix& similarityTable);
    void buildSimilarityTable(Matrix& similarityTable);
    U32 buildwordIndexTable(const vector<string>& wordSequence, wordIndexTable& indexTable);
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
    U32 buildwordIndexTable(const vector<string>& wordSequence);
    void validateWordKeyTables(void);
    void clearwordIndexTable(void);
    U32 allocId(const string& word);
    U32 allocId(const string& word, WordIndexTableIt it);
    string idToStr(const U32 id);
    U32 strToId(const string& word);

    void printTopWords(int n);

    //void copyVecs(const vector<string>& src, vector<string>& dest);
    void prunewordIndexTable(WordIndexTable& wordTable);
    bool ShareCoOccurrences(Matrix& simTable, RowMap& row, U32 key1, U32 key2);
    bool CoDistTableHasKey(Matrix& coDistTable, const string& k1, const string& k2, MatrixInnerIt& it);
    bool CoDistTableHasKey(Matrix& coDistTable, const U32 key1, const U32 key2, MatrixInnerIt& it);
    void updateCoDistTable(const string& key1, const string& key2);
    void coDistTableToFile(string& fname);
    FloatType getSumDistance(const string& w1, const string& targets);
    FloatType getClusterDistance(U16 clusterNo, const string& postStr);
    //U32 getSimilarity(const string& w1, const string& w2); //old static version: returned nAB
    FloatType getSimilarity(const string& w1, const string& w2);
    //double getSumSimilarity(const U32 w1);
    FloatType getSumSimilarity(const string& w1);
    FloatType calculateSignificance(const U32 w1, const U32 w2);
    void buildCoOccurrenceMatrix(vector<string>& wordSequence);
    void buildCoDistTableFromFile(const string& fname);
*/
    void printDotProductVector(const DotProductVector& vec);
    void buildSimilarityModel(const string& trainingFile, Matrix& simMatrix);
    void matrixDotProduct(Matrix& bigMatrix, Matrix& smallerMatrix, DotProductVector& resultant);
    void buildCoOccurrenceMatrix(vector<string>& wordSequence, WordIndexTable& wordIndexTable, Matrix& coDistMatrix);
    void buildSimilarityMatrix(Matrix& coOccurrenceMatrix, Matrix& similarityMatrix);
    U32 buildWordIndexTable(const vector<string>& wordSequence, WordIndexTable& wordIndexTable);
    FloatType method_binaryCosine(RowMap& row1, RowMap& row2);
    FloatType method_weightedCosine(const RowMap& row1, const RowMap& row2);
    void updateCoOccurrenceMatrix(const string& key1, const string& key2, Matrix& coDistMatrix);
    //bool shareCoOccurrences(Matrix& simTable, U32 key1, U32 key2);
    bool shareCoOccurrences(MatrixOuterIt& row1, MatrixOuterIt& row2);
    bool matrixHasKey(Matrix& matrix, const U32 key1, const U32 key2, MatrixInnerIt& it);
    bool matrixHasKey(Matrix& matrix, const string& k1, const string& k2, MatrixInnerIt& it);
    void matrixToFile(string& fname, const Matrix& matrix);

  public:
    PersonalClassifier();
    PersonalClassifier(const string& stopWordFile);
    ~PersonalClassifier();
    void testPerson(const string& personalTrainingFile, const string& personalSampleFile);
};

































