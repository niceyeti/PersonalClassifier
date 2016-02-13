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
    double doSum(DotProductVector& dotProduct);

  public:
    PersonalClassifier();
    PersonalClassifier(const string& stopWordFile);
    ~PersonalClassifier();
    string getFileName(const string& fullFilePath);
    string getDirName(const string& fullFilePath);
    void testPerson(const string& personalTrainingFile, Matrix& testMatrix, DotProductVector& dotProduct);
    void classify(const string& personnelDir, const string& testSampleFile);
};

































