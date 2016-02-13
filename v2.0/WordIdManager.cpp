#include "WordIdManager.hpp"

WordIdManager::WordIdManager()
{
  m_idCounter = 0;
}

WordIdManager::~WordIdManager()
{
  m_idTable.clear();
  m_wordTable.clear();
}

U32 WordIdManager::getIdCounter(void)
{
  return m_idCounter;
}

void WordIdManager::updateWordIdModel(vector<string>& wordStream)
{
  int res, i;
  bool fail = false;

  for(i = 0; i < wordStream.size() && !fail; i++){
    res = allocId(wordStream[i]);
    if(res == 0){
      cout << "ERROR out of U32 word-id keys" << endl;
      fail = true;
    }
  }
}

//Given some word, alloc an id for it in our vocab tables; if already present, return its existing id
U32 WordIdManager::allocId(const string& word)
{
  U32 ret;
  WordTableIt it = m_wordTable.find(word);

  if(m_idCounter >= U32MAX){
    cout << "ERROR out of id's in allocId" << endl;
    return 0;
  }

  if(it == m_wordTable.end()){
    //update both tables, so they are always in sync
    ++m_idCounter;
    m_idTable[m_idCounter] = word;
    m_wordTable[word] = m_idCounter;
    ret = m_idCounter;
  }
  else{
    ret = it->second; //return existing id if word is already in table
  }

  return ret;
}

/*
  Finds word in id table; if not found, attempts to alloc a new id.
*/
U32 WordIdManager::stringToId(const string& word)
{
  U32 id;
  WordTableIt it = m_wordTable.find(word);

  if(it == m_wordTable.end()){
    //cout << "WARN word-id not found for >" << word << "< attempting to alloc new id. Verify m_wordTable and m_idTable alignment." << endl;
    id = allocId(word);
    if(id == 0){
      cout << "ERROR out of keys in getId()" << endl;
    }
  }
  else{
    id = it->second;
  }

  return id;
}

//convert a u32 id back to its string
string WordIdManager::idToString(const U32 id)
{
  string word;
  IdTableIt it = m_idTable.find(id);

  if(it == m_idTable.end()){
    cout << "ERROR word-id not found for >" << word << "< in IdToStr()" << endl;
    word = "NIL";
  }
  else{
    word = it->second;
  }

  return word;
}

//verifies that tables have equal key-sets; eg, S1 is subset of S2, and S2 is subset of S1
void WordIdManager::validateWordKeyTables(void)
{
  string s;
  U32 id;
  WordTableIt wordIt = m_wordTable.begin();
  IdTableIt idIt = m_idTable.begin();

  cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~Beginning word key/id tables validation~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;

  if(m_wordTable.size() != m_idTable.size()){
    cout << "ERROR in validateWordKeyTables(), m_wordTable.size()=" << m_wordTable.size() << " != " << "m_idTable.size()=" << m_idTable.size() << endl;
  }

  //verify membership: all keys in m_wordTable in m_idTable AND that their values are correctly mapped
  for(idIt = m_idTable.begin(); idIt != m_idTable.end(); ++idIt){
    s = idIt->second;
    id = idIt->first;

    wordIt = m_wordTable.find(s);
    if(wordIt == m_wordTable.end()){
      cout << "1 ERROR s/id=" << s << "/" << id << " in m_idTable but not found in m_wordTable" << endl;
    }
    else if(wordIt->first != s){
      cout << "1 ERROR strings not equal from m_wordTable and m_idTable: " << wordIt->first << "/" << s << endl;
    }
    else if(wordIt->second != id){
      cout << "1 ERROR ids not equal from m_wordTable and m_idTable: " << wordIt->second << "/" << id << endl;
    }
  }

  //verify membership: all keys in m_idTable in m_wordTable AND that their values are correctly mapped
  for(wordIt = m_wordTable.begin(); wordIt != m_wordTable.end(); ++wordIt){
    s = wordIt->first;
    id = wordIt->second;

    idIt = m_idTable.find(id);
    if(idIt == m_idTable.end()){
      cout << "2 ERROR word=" << s << " from m_wordTable not found in m_idTable" << endl;
    }
    else if(idIt->second != s){
      cout << "2 ERROR strings not equal from m_idTable and m_wordTable: " << idIt->first << "/" << s << endl;
    }
    else if(idIt->first != id){
      cout << "2 ERROR ids not equal from m_idTable and m_wordTable: " << idIt->first << "/" << id << endl;
    }
  }
  cout << "Word id/key tables verification PASSED, iff no ERROR messages above. If so, tables are sync" << endl;
  cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
}




/*
  Inits the word-id key relationships in both the wordIndex Table (word->key) and the idTable (U16->word)

void kNN::initm_idTable(void)
{
  if(m_wordTable.empty()){
    cout << "ERROR m_wordTable empty in initm_wordTableKeys" << endl;
  }
  if(!m_idTable.empty()){
    m_idTable.clear();
  }

  idCounter = 0;
  //guarantees m_idTable keys and m_wordTable keys are in sync.
  for(WordTableIt it = m_wordTable.begin(); it != m_wordTable.end(); ++it){
    allocId(it->first, it);
    if(idCounter >= U32MAX){
      cout << "ERROR out of U32 word-id keys" << endl;
      return;
    }
  }
  cout << "m_idTable init completed, idCounter=" << idCounter << endl;

  validateWordKeyTables();
}
*/


