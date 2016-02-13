#ifndef HEADER_HPP
  #include "Header.hpp"
#endif

#ifndef WORDIDMANAGER_HPP
#define WORDIDMANAGER_HPP


/*
  This class offers a more compact way of storing items by id, rather than value. For instance, instead of storing
  a 3-gram n-gram word model with strings, replace the strings with unique integer id's. Then just convert back and
  forth between strings and id's as needed, which is the storage and service which this class provides.

  If the max number of unique items is less than MAX_U16, MAX_U32, etc, you can modify this class to operate on different
  types, and reduce mem consumption even further, considering wherever else the id's may be stored (such as in a giant
  Markov n-gram model, as mentioned above). This could/should be a templated class depending on the id-type parameter,
  but I've hardcoded U32 id's since it encompasses most cases anyway. Note the use of native ints also avoids the hidden
  byte-overhead with objects types, like std::string, which use more bytes than the bytes given by the string length.
*/

typedef unordered_map<string,U32> WordTable;
typedef WordTable::iterator WordTableIt;
typedef unordered_map<U32,string> IdTable;
typedef IdTable::iterator IdTableIt;

class WordIdManager{

  private:
    U32 m_idCounter;
    IdTable m_idTable;
    WordTable m_wordTable;
    void validateWordKeyTables(void);

  public:
    WordIdManager();
    ~WordIdManager();

    U32 getIdCounter(void);
    void updateWordIdModel(vector<string>& wordStream);
    U32 allocId(const string& word);
    U32 stringToId(const string& word);
    string idToString(const U32 id);
};

#endif





