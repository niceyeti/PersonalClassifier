#ifndef HEADER_HPP
  #include "Header.hpp"
#endif

#ifndef TEXTPROCESSOR_HPP
#define TEXTPROCESSOR_HPP

class TextProcessor{

  private:
    unordered_set<string> m_stopWords;
    string dbfile;
    string fileDelimiter;
    string phraseDelimiters;
    string wordDelimiters;
    string delimiters;
    string rawDelimiters;
    char wordDelimiter;
    char phraseDelimiter;
    U32 idCounter;
    U64 vecCt;
    U64 numwords;
    bool validDecimalChars[256];

  public:
    TextProcessor();
    TextProcessor(const string& stopWordFile);
    ~TextProcessor();

    void textToWordSequence(const string& trainingFile, vector<string>& wordSequence, bool filterStopWords);
    void buildStopWordTable(const string& stopWordFile);
    bool isStopWord(const char* word);
    bool isStopWord(const string& word);
    bool isValidWordExtended(const char* word);
    int findChar(const char buf[], char c, int len);
    int seekLastPhraseDelimiter(const char buf[BUFSIZE], int len);
    bool isPhraseDelimiter(char c);
    void normalizeText(char ibuf[BUFSIZE], string& ostr);
    void delimitText(string& istr);
    bool isWordDelimiter(char c);
    void scrubHyphens(string& istr);
    void finalPass(string& buf);
    void toLower(string& myStr);
    void toLower(char buf[BUFSIZE]);
    void rawPass(string& istr);
    bool isDelimiter(const char c, const string& delims);
    int tokenize(char* ptrs[], char buf[BUFSIZE], const string& delims);
    bool isInt(const string& nstr);
    bool isDecimal(const string& nstr);
    bool hasColon(const char buf[], int len);
    void initValidDecimalChars(void);
    bool isValidWordExtended(const string& token);
    bool isValidWord(const char* word);
    bool isValidWord(const string& token);
};

#endif
