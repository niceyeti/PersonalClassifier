#include "kNN.hpp"

/*
  For similarity/distance measures, there are many options. Many links on wikis as well. See Jaccard index. Could
  be a useful metric for computing similarity of word-history to some context (sports, politics, etc)

  Min-spanning trees and other data structures may be used for clustering.



knn source stuff:
http://stackoverflow.com/questions/13769242/clustering-words-into-groups

  -if we decide to use datasets longer than max(uint32) (> ~4 billion), then replace all U32 variables here with U64 to make sure all
  possible index values can fit in such variables. This is primarily important for the CoDistTable() and functions related to it.
  -if we're not eating up a ton of memory for the CoDistTable and such, just ctrl+h U32 for U64 anyway
  - could also log2() each distance value, as:  (U32)log2((double)dist).


  Weekend: NEED stable dataset, not enron.


  TODO: build bitmaps instead of testing numerical ranges:
  index into array of length 0-255 (range of U8), using char as index. value of cell is bool: VALID or INVALID

*/

/*
bool compstrs()
{
  return 
}
*/
bool byFrequency(const pair<string,long double>& left, const pair<string,long double>& right)
{
  return left.second > right.second;
}

bool bySimilarity(const pair<string,double>& left, const pair<string,double>& right)
{
  return left.second > right.second;
}

bool byFrequency_U32(const pair<string,U32>& left, const pair<string,U32>& right)
{
  return left.second > right.second;
}

bool compareCoDistTableIt(const pair<string,U32>& p1, const pair<string,U32>& p2)
{
  return p1.second < p2.second;
}

/*
  Modify this function to alter the distance metric.

  Used to sort ascending for prev distance algorithm, since we wanted the min-distance
  elements at the front of the list since they're more desirable.
  Currently sorts by max distance, which is nicer because its more in line with max-scoring instead of min-scoring.
*/
bool byNearestWords(const pair<VecListIt,Delta>& left, const pair<VecListIt,Delta>& right)
{
  return left.second.semanticDist >= right.second.semanticDist;
}

inline U64 argmax(U64 left, U64 right)
{
  if(left > right){
    return left;
  }
  return right;
}

inline U64 argmin(U64 left, U64 right)
{
  if(left < right){
    return left;
  }
  return right;
}

inline U32 argmax(U32 left, U32 right)
{
  if(left > right){
    return left;
  }
  return right;
}

inline U32 argmin(U32 left, U32 right)
{
  if(left < right){
    return left;
  }
  return right;
}

kNN::kNN()
{
  //dbfile = "/home/jesse/Desktop/TeamGleason/corpii/coocDB.txt";
  dbfile = "../coocWeightedCosine.txt";
  phraseDelimiters = "\".?!#;:)(";  // octothorpe is user defined
  rawDelimiters = "\"?!#;:)(, "; //all but period
  wordDelimiters = ", ";
  delimiters = phraseDelimiters;  //other special chars? could be useful for technical texts, eg, financial reports
  delimiters += wordDelimiters;
  //delimiters += "'";
  wordDelimiter = ' ';
  phraseDelimiter = '#';

  initValidDecimalChars();
  clearScore();

  meansFile = "./kMeans.txt";
  kMeansFile = "./kMeans.txt";

  string s = "../stopWords1.txt";
  //string s = "../stopWordsExtended.txt";
  buildStopWordTable(s);
  numwords = 0;
  vecCt = 0;
  idCounter = 0;
  clusterIdCounter = 0;

  //s = "..someEnronData.txt"
  //buildVectorTable(s);
}

kNN::~kNN()
{
  cout << "destroying kNN" << endl;
  //nothing to clean up. STL structures' destructors will be called.
}

void kNN::initValidDecimalChars(void)
{
  for(int i = 0; i < 256; i++){
    validDecimalChars[i] = false;
  }
  validDecimalChars['0'] = true;
  validDecimalChars['1'] = true;
  validDecimalChars['2'] = true;
  validDecimalChars['3'] = true;
  validDecimalChars['4'] = true;
  validDecimalChars['5'] = true;
  validDecimalChars['6'] = true;
  validDecimalChars['7'] = true;
  validDecimalChars['8'] = true;
  validDecimalChars['9'] = true;
  validDecimalChars['E'] = true;
  validDecimalChars['e'] = true;
  validDecimalChars['-'] = true;
  validDecimalChars['+'] = true;
  validDecimalChars['.'] = true;
}




/*
  Routine for getting line from frequency file. Chops buf at newline,
  and sets file position to the next line that is also not a newline.
  
  Note this function may be slow if a file contains many empty lines (repeated \n sequences).
*/
char* kNN::getLine(int openfd, char buf[], int maxlen)
{
  int n, i;

  n = read(openfd,buf,maxlen);
  if(n <= 0){
    //cout << "WARN read() returned >" << n << "< in getLine(). Returning NULL." << endl;
    return NULL;
  }

  /*
    Reset file position to position of first newline char
    If we read n chars, then we need to back up to 'pos' index,
    which is back from the current position by (n - pos)
  */
  i = findChar(buf,'\n',n);
  if(i == -1){
    cout << "ERROR findChar(\\n) returned -1 in getLine(). Returning NULL." << endl;
    buf[0] = '\0';
    return NULL;
  }
  buf[i] = '\0';

  i = i - n + 1;  //position of first newline + 1

  if(i < 0){
    lseek(openfd, i, SEEK_CUR);
  }

  return buf;
}


/*
  Small utility for finding the position of a char in some input buffer.
  Used when we need to lseek() back to some position in a file to verify we're
  reading delimiter-length segments.

  Returns: position of char c, or -1 if not in string.
*/
int kNN::findChar(const char buf[], char c, int len)
{
  int i;

  if((buf == NULL) || (len <= 0)){  
    cout << "ERROR buf null or len < 0 in findChar()" << endl;
    return 0;
  }

  for(i = 0; buf[i] && (i < len) && (buf[i] != c); i++){ /*empty*/ }

  if((i == len) || ((buf[i] == '\0') && (c != '\0'))){
    i = -1;
  }

  return i;
}

/* 
   Find and return index of leftmost, last delimiter. This tells us where
   the last regular structure (sentence, phrase, etc) ended. This
   will be used to chop the input string in a logical manner between
   read() operations.

  Examples:
     "abc###" - returns  3
     "abcde#" - returns  5
     "abcde"  - returns -1

  Returns index of leftmost delimiter last delimiter (-1 if none found).
*/
int kNN::seekLastPhraseDelimiter(const char buf[BUFSIZE], int len)
{
  int i;

  //starting at end of string, spool left to rightmost of group of phrase delimiters
  for(i = len-1; (i >= 0) && !isPhraseDelimiter(buf[i]); i--){}
  //end loop: i points at a phrase delimiter OR i == -1

  // currently at last/rightmost delim. now spool to leftmost phrase delim in this group
  for( ; (i >= 0) && isPhraseDelimiter(buf[i]); i--){}
  //end loop: i==-1 OR buf[i] NOT_IN delimiters[]

  //we are off-by-one after i-- stmt in the last loop to execute, so increment by one
  if(i > 0){
    i++;
  }

  //cout << "last delimiter is >" << buf[i] << "<" << endl;

  return i;
}

bool kNN::isPhraseDelimiter(char c)
{
  U32 i;

  /*
  if(c == 34){
    return true;
  }
  */

  for(i = 0; (phraseDelimiters[i] != '\0') && (i < phraseDelimiters.length()); i++){
    if(c == phraseDelimiters[i]){
      return true;
    }
  }

  return false;
}



/*
  Do our best to clean the sample. We try to preserve as much of the author's style as possible,
  so for instance, we don't expand contractions, viewing them instead as synonyms. "Aren't" and
  "are not" thus mean two different things.

  ASAP change params to both string
*/
void kNN::normalizeText(char ibuf[BUFSIZE], string& ostr)
{
  string istr = ibuf; //many of these are redundant
  //string ostr;

  /*
  cout << len << " (first 120ch) length input sample looks like: (chct=" << len << "," << strlen(buf) << "):" << endl;
  for(i = 0; i < 120; i++){
    putchar(buf[i]);
  }
  cout << "<end>" << endl;
  */

  //non-context free transformer pipeline
  //TODO: map abbreviations.
  //mapAbbreviations(istr,ostr);  //demark Mr. Ms., other common abbreviations before rawPass() uses periods as phrase delimiters
  //post: "Mr." will instead be "Mr+". We'll use '+' to convert back to "Mr." after calling delimitText() 
  //mapContractions(obuf.c_str(), obuf2);
  //dumb for now. obuf will be used later for more context-driven text preprocessing

  //filters and context-free transformers
  rawPass(istr);
  //cout << "1: " << buf << endl;
  toLower(istr);
  //cout << "2: " << buf << endl;
  scrubHyphens(istr);
  //cout << "3: " << buf << endl;
  delimitText(istr);
  //cout << "4: " << buf << endl;
  finalPass(istr);

  ostr = istr; //note this effectively clears any previous contents of ostr, which is intended

  //cout << "here: " << buf[0] <<  endl;
  /*
  cout << "120ch output sample looks like: " << endl;
  for(i = 0; i < 120; i++){
    putchar(buf[i]);
  }
  cout << "<end>" << endl;
  cin >> i;
  */
}

/*
  Replaces delimiter chars with either phrase (#) or word (@) delimiters.
  This is brutish, so input string must already be preprocessed.
  Output can be used to tokenize phrase structures and words.


  Notes: This function could be made more advanced. Currently it makes direct replacement
  of phrase/word delimiters with our delimiters (without regard to context, etc)
*/
void kNN::delimitText(string& istr)
{
  int i, k;

  for(i = 0; istr[i] != '\0'; i++){
    if(isPhraseDelimiter(istr[i])){  //chop phrase structures
      istr[i] = phraseDelimiter;
      
      //consume white space and any other delimiters (both phrase and words delims):  "to the park .  Today" --becomes--> "to the park####Today"
      k = i+1;
      while((istr[k] != '\0') && isDelimiter(istr[k],this->delimiters)){
        istr[k] = phraseDelimiter;
        k++;
      }
    }
    else if(isWordDelimiter(istr[i])){
      istr[i] = wordDelimiter;

      //consume right delimiters
      k = i+1;
      while((istr[k] != '\0') && isWordDelimiter(istr[k])){
        istr[k] = wordDelimiter;
        k++;
      }
    }
  }
}

bool kNN::isWordDelimiter(char c)
{
  int i;

  for(i = 0; (wordDelimiters[i] != '\0') && (i < wordDelimiters.length()); i++){
    if(c == wordDelimiters[i]){
      return true;
    }
  }

  return false;
}



/*
  Hyphens are ambiguous since they can represent nested phrases or compound words:
    1) "Mary went to the park--ignoring the weather--last Saturday."
    2) "John found a turquoise-green penny."
  Process string by checking for hyphens. Double hyphens represent nested
  phrases, so will be changed to phrase delimiter. Single hyphens will
  be changed to a word-delimiter (a space: ' ').

  Notes: this function snubs context. Huck Finn contains a lot of hyphens, without
  much regular structure (except double vs. single). More advanced parsing will be needed
  to preserve nested context: <phrase><hyphen-phrase><phrase>. Here the first and second <phrase>
  are severed contextually if we just shove in a delimiter. Re-organizing the string would be nice,
  but also difficult for parses like <phrase><hyphen-phrase><hyphen-phrase><hyphen-phrase><phrase> 
  or <phrase><hyphen-phrase><phrase><hyphen-phrase><phrase> which occur often in Huck Finn.
*/
void kNN::scrubHyphens(string& istr)
{
  int i;

  for(i = 0; istr[i] != '\0'; i++){
    if((istr[i] == '-') && (istr[i+1] == '-')){  //this snubs nested context
      istr[i+1] = istr[i] = phraseDelimiter;
    }
    else if(istr[i] == '-'){   //this could also use more context sensitivity: eg, "n-gram" should not necessarily resolve to "n" and "gram" since n is not a word
      istr[i] = wordDelimiter;
    }
  }
}

/*
  Convert various temp tags back to their natural equivalents.
  For now, just converts "Mr+" back to the abbreviation "Mr."
*/
void kNN::finalPass(string& buf)
{
  for(int i = 0; i < buf.length(); i++){
    if(buf[i] == PERIOD_HOLDER){
      buf[i] = '.';
    }
  }
}

void kNN::toLower(string& myStr)
{
  for(int i = 0; i < myStr.length(); i++){
    if((myStr[i] >= 'A') && (myStr[i] <= 'Z')){
      myStr[i] += 32;
    }
  }
}

//standardize input by converting to lowercase
void kNN::toLower(char buf[BUFSIZE])
{
  int i;

  for(i = 0; buf[i] != '\0'; i++){
    if((buf[i] >= 'A') && (buf[i] <= 'Z')){
      buf[i] += 32;
      //cout << buf[i] << " from " << (buf[i] -32) << endl;
    }
  }
}

/*
  Raw char transformer. currently just replaces any newlines or tabs with spaces. And erases "CHAPTER" headings.
  changes commas to wordDelimiter
*/
void kNN::rawPass(string& istr)
{
  int i;

  for(i = 0; istr[i] != '\0'; i++){
    if((istr[i] < 32) || (istr[i] > 122)){ // convert whitespace chars and extended range chars to spaces
    //if((istr[i] == '\n') || (istr[i] == '\t') || (istr[i] == '\r') || (istr[i] == ',')){
      istr[i] = wordDelimiter;
    }
    else if(istr[i] == ','){   //HACK
      istr[i] = wordDelimiter;
    }

    /*
    //HACK erase "CHAPTER n." from input, a common header in Hucklberry Finn
    // left two checks short-circuit to only allow call strncmp if next two letters are "CH" (ie, high prob. of "CHAPTER")
    if(((i + 16) < len) && (istr[i] == 'C') && (istr[i+1] == 'H') && !strncmp("CHAPTER",&istr[i],7)){
      j = i + 8;
      for( ; (i < j) && (i < len); i++){
        istr[i] = phraseDelimiter;
      }
      //now we point at 'X' in "CHAPTER X", so consume chapter numerals until we hit the period
      for( ; (istr[i] != '.') && (i < len); i++){
        istr[i] = phraseDelimiter;
      }
      if(i == len){
        cout << "ERROR istrfer misalignment in rawPass()" << endl;
      }
      else{
        istr[i++] = phraseDelimiter;  
      }
    }
    */
  }
}

/*
  This is the most general is-delim check:
  Detects if char is ANY of our delimiters (phrase, word, or other/user-defined.)
*/
bool kNN::isDelimiter(const char c, const string& delims)
{
  int i;

  for(i = 0; i < delims.length(); i++){
    if(c == delims[i]){
      return true;
    }
  }

  return false;
}



/*
  Logically the same as strtok: replace all 'delim' chars with null, storing beginning pointers in ptrs[]
  Input string can have delimiters at any point or multiplicity

  Pre-condition: This function continues tokenizing until it encounters '\0'. So buf must be null terminated,
  so be sure to bound each phrase with null char.

  Testing: This used to take a len parameter, but it was redundant with null checks and made the function 
  too nasty to debug for various boundary cases, causing errors.
*/
int kNN::tokenize(char* ptrs[], char buf[BUFSIZE], const string& delims)
{
  int i, tokCt;
  //int dummy;

  if((buf == NULL) || (buf[0] == '\0')){
    ptrs[0] = NULL;
    cout << "WARN buf==NULL in tokenize(). delims: " << delims << endl;
    return 0;
  }
  if(delims.length() == 0){
    ptrs[0] = NULL;
    cout << "WARN delim.length()==0 in tokenize()." << endl;
    return 0;
  }

  i = 0;
  if(isDelimiter(buf[0], delims)){
    //consume any starting delimiters then set the first token ptr
    for(i = 0; isDelimiter(buf[i], delims) && (buf[i] != '\0'); i++);
    //cout << "1. i = " << i << endl;
  }

  if(buf[i] == '\0'){  //occurs if string is all delimiters
    if(DBG)
      cout << "buf included only delimiters in tokenize(): i=" << i << "< buf: >" << buf << "< delims: >" << delims << "<" << endl;
    ptrs[0] = NULL;
    return 0;
  }

  //assign first token
  ptrs[0] = &buf[i];
  tokCt = 1;
  while(buf[i] != '\0'){

    //cout << "tok[" << tokCt-1 << "]: " << ptrs[tokCt-1] << endl;
    //cin >> dummy;
    //advance to next delimiter
    for( ; !isDelimiter(buf[i], delims) && (buf[i] != '\0'); i++);
    //end loop: buf[i] == delim OR buf[i]=='\0'

    //consume extra delimiters
    for( ; isDelimiter(buf[i], delims) && (buf[i] != '\0'); i++){
      buf[i] = '\0';
    } //end loop: buf[i] != delim OR buf[i]=='\0'

    //at next substring
    if(buf[i] != '\0'){
      ptrs[tokCt] = &buf[i];
      tokCt++;
      
/*
      if(tokCt % 200 == 0){
        cout << "tokCt=" << tokCt <<  flush;
      }
*/
    }

  } //end loop: buf[i]=='\0'

  //cout << "DEBUG first/last tokens: " << ptrs[0] << "/" << ptrs[tokCt-1] << "<end>" <<  endl; 

  ptrs[tokCt] = NULL;

  return tokCt;
}




/*
  Searches an open file for first instance of datetime pattern: dd/dd/dddd dd:dd aa   eg, 10/12/2000 6:56 PM
*/
DateTime kNN::searchDateTime(int iFile)
{
  char buf[BUFSIZE];
  DateTime datetime = {255,255,255,255};

  while(getLine(iFile,buf,256) && (datetime.hr == 255)){
    datetime = strToDateTime(buf);
  }

  return datetime;
}

//cleanup. this probably isn't necessary, if wordIndexTable.clear() is sufficient to clear the vectors in contains
void kNN::clearWordIndexTable(void)
{
  if(wordIndexTable.empty()){
    return;
  }

  for(IndexTableIt it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    it->second.second.clear(); //clear all the vectors
  }
  wordIndexTable.clear();
}

timespec kNN::DiffTimeSpecs(timespec* start, timespec* end)
{
	timespec temp;
  temp.tv_sec = temp.tv_nsec = 0.0;

	if((end->tv_nsec - start->tv_nsec) < 0) {
		temp.tv_sec = end->tv_sec - start->tv_sec - 1;
		temp.tv_nsec = 1000000000+ end->tv_nsec - start->tv_nsec;
	}
  else{
		temp.tv_sec = end->tv_sec - start->tv_sec;
		temp.tv_nsec = end->tv_nsec - start->tv_nsec;
	}

	return temp;
}


void kNN::train(const string& trainingFile)
{
  int inFile, i; //, n, lastDelim, dummy;
  U64 bytesRd, maxlen;
  DateTime utime;
  //long double progress;
  //long double gramCt, rawHitCt, realHitCt, result, progress;
  struct stat fileInfo, dbFileInfo;
  //char buf[BUFSIZE];
  string obuf(BUFSIZE,'\0');
  vector<string> wordVec, filteredSequence, exampleVec;
  
  //debug
  //time_t tm;

  //bad use of patterns, one using linux open() call, another using fstream internally
  inFile = open(trainingFile.c_str(), O_RDONLY);
  if(inFile < 0){
    cout << "ERROR could not open input file >" << trainingFile << "< testModel() aborted" << endl;
    return;
  }
  if(fstat(inFile, &fileInfo)){
    cout << "ERROR fstat() call failed in testModel()" << endl;
    return;
  }
  if(stat(dbfile.c_str(), &dbFileInfo)){
    cout << "ERROR fstat() call failed in testModel()" << endl;
    return;
  }
  //utime = searchDateTime(inFile);
  close(inFile);
  cout << "file: " << trainingFile << " dateTime: " << (int)utime.hr << "h " << (int)utime.day << "d " << (int)utime.month << "mo " << ((int)utime.year+1970) << "y" << endl;

  /*
  Builds the basis of words for the models.
  1) get all non-stop words as a linear sequence
  2) build a table of these words, where word->[i,j,k...] where i,j,k are indices of the word in the sequence
  3) the word-distribution has a heavy/long tail, so prune the table based on some pruning threshold
  4) Step 3) leaves pruned words in the sequence, so filter these and then rebuild the table from the filtered words

  End condition: a linear sequence of words and an index table of those words (which also gives their frequency, via word.|indices|
  So we based our models off of a pruned set of words, only to reduce memory complexity. Algorithmic complexity for kMeans and CoDistance
  calculations have no problem handling millions of words; the problem is that the matrix of word relations is n x n, which doesn't fit in memory
  if the set is not pruned.
  */
/*  //get all the (filtered) words in the training file
  textToWordSequence(trainingFile,wordVec,true);
  //copyVecs(wordVec,exampleVec);
  maxlen = buildWordIndexTable(wordVec);
  //cout << "before prune, wordIndexTable.size()=" << wordIndexTable.size() << endl;
  pruneWordIndexTable(wordIndexTable); //pruning wordIndexTable is a pre-condition for calling pruneWordSequence()
  pruneWordSequence(wordVec,filteredSequence);  //align word sequence with pruned wordIndexTable, to prevent a lot of out-of-vocabulary noise
  clearWordIndexTable();
  //cout << "wordIndexTable cleared, filteredSequence.size()=" << filteredSequence.size() << endl;
  buildWordIndexTable(filteredSequence); //rebuild the table based on filtered sequence, so index lists are valid for this sequence
  initWordIdTable();
*/

//getVectors();
//updateVectorTable(wordVec,utime);

  //printTopWords(500);
  cout << "after seq prune, wordIndexTable.size()=" << wordIndexTable.size() << endl;
  //cout << "post textToWordSequence(), size is:" << wordVec.size() << " words" << endl;

  textToWordSequence(trainingFile,wordVec,false);
  maxlen = buildWordIndexTable(wordVec);
  initWordIdTable();
  vecCt += updateVectorTable(wordVec,utime);
  cout << "before clearing excess vectors, wordIndexTable.size() is " << wordIndexTable.size() << endl;
  for(IndexTableIt it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    it->second.second.clear();
  }
  cout << "after clearing excess vectors, wordIndexTable.size() is " << wordIndexTable.size() << endl;


  //struct timespec begin, end;
  //long double scalar = 1000000000.0;
  //clock_gettime(CLOCK_MONOTONIC,&begin);
//  buildCoDistTable(filteredSequence);
buildCoDistTableFromFile(dbfile);
  //clock_gettime(CLOCK_MONOTONIC,&end);
  //end = DiffTimeSpecs(&begin,&end);
  //cout << "co dist build() time: " << end.tv_sec << "s " << end.tv_nsec << "ns" << endl;

  //cout << "codist build() runtime: " << ((long double)(end.tv_nsec - begin.tv_nsec) / scalar) << endl;
  //cout << "end_sec " << end.tv_sec << "  end_nsec " << end.tv_nsec << "  start_sec " << begin.tv_sec << "  start_nsec " << begin.tv_nsec << endl;


/*  i = 0;
  for(vector<string>::iterator it = wordVec.begin(); i < 200 && it != wordVec.end(); ++it, i++){
    cout << *it << " ";
  }
  cout << "\nsize=" << wordVec.size() << endl;
  cin >> i;
*/
  //buildWordIndexTable(wordVec);
  //pruneWordIndexTable(wordIndexTable);
/*
  buildCoDistTableFromFile(dbfile);
  vecCt = 0;
  cout << "pre updateVectorTable.size()=" << vectorTable.size() << endl;
  vecCt += updateVectorTable(wordVec,utime);
  string s = "../vecTable.txt";
  //getVectors(s);
  wordVec.clear();
  cout << "post updateVectorTable(), table.size()=" << vectorTable.size() << endl;
*/
/*
  //now build the coDistTable~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  if(dbFileInfo.st_size > 10000){  //if dbfile has less than 10k bytes (loosely an insufficient model size), assume it has no model, and build from scratch
    cout << "building coDistTable from file..." << endl;
    buildCoDistTableFromFile(dbfile);  //DEAD CODE
  }
  else{  //building from scratch is not too slow, but building from file is much faster
    buildCoDistTable(filteredSequence);
  }
*/

/*
  buildCoDistTableFromFile(dbfile);
    cout << "testing get synonyms on >difficult<";
    list<pair<U32,double> > synonyms;
    getSynonyms("difficult",synonyms,10);
    for(list<pair<U32,double> >::iterator it = synonyms.begin(); it != synonyms.end(); ++it){
      cout << "  >>" << idToStr(it->first) << endl;
    }
*/
  //buildCoDistTable(filteredSequence);

  //buildClusterModels();

  /*
  VectorTableIt it = vectorTable.begin();
  for(i = 0; it != vectorTable.end() && i < 100; ++it, i++){
    cout << i << ": " << it->first << "->" << it->second.begin()->postStr << endl;
  }
  cin >> buf[0];
  */

/*
  //dbg
  i = 0;
  for(VectorTableIt it = vectorTable.begin(); i < 1000 && it != vectorTable.end(); i++, ++it){
    cout << it->first << "|listsz=" << it->second.size() << endl; 
  }
*/
  cout << "Train(" << trainingFile << ") complete. vecCt=" << vecCt << " vecTable.size()=" << vectorTable.size() << endl;
}

/*
  Driver for running kmeans fresh, or just reading previous output from a file.

  pre-condition: wordIndex table and CoDistTables are built

  post: some k-means are restored. The cluster members themselves are not, unless kMeans is run.
*/
void kNN::buildClusterModels(void)
{
  struct stat fileInfo;

  if(stat(meansFile.c_str(), &fileInfo)){
    cout << "ERROR fstat() call failed in buildClusterModel()" << endl;
    return;
  }

  //if file is some pre-existing size, presume it contains a valid model and use it instead of rebuilding it
  if(fileInfo.st_size > 10000){
    recallMeansFromFile();
  }
  else{
    //kMeansSoft(KMEANS);
    cosineClustering(wordIdTable.size());
  }

  cout << "kmeans done." << endl;

}


/*
  string test = "block";
  it = vectorTable.find(test);
  if(it == vectorTable.end()){
    cout << "NOT FOUND" << endl;
  }
  else{
    cout << "FOUND: " << test << " >" << it->second.begin()->postStr << "< (size=" << it->second.size() << ")" << endl;
  }
  cin >> buf[0];
*/


/*
  Builds vectors from a set of training data.

  Each vector is defined by:
    <preceding three words, featureList[] >

void kNN::train(const string& trainingFile)
{
  int inFile, i, n, lastDelim, dummy;
  U64 bytesRd;
  DateTime utime;
  long double gramCt, rawHitCt, realHitCt, result, progress;
  struct stat fileInfo;
  char buf[BUFSIZE];
  string obuf(BUFSIZE,'\0');
  vector<string> wordBuf;
  //debug
  //time_t tm;

  inFile = open(trainingFile.c_str(), O_RDONLY);
  if(inFile < 0){
    cout << "ERROR could not open input file >" << trainingFile << "< testModel() aborted" << endl;
    return;
  }
  if(fstat(inFile, &fileInfo)){
    cout << "ERROR fstat() call failed in testModel()" << endl;
    return;
  }
  
  utime = searchDateTime(inFile);
  cout << "file: " << trainingFile << " dateTime: " << (int)utime.hr << "h " << (int)utime.day << "d " << (int)utime.month << "mo " << ((int)utime.year+1970) << "y" << endl;

  gramCt = bytesRd = 0; i = 0;
  //we read discrete phrases structures by reading 1024 bytes, then backing file position to the last delimiter
  while(n = read(inFile, buf, READ_SZ)){
    bytesRd += n;
    //i++;
    //if((i %= 2) == 0){
      progress = 101 * ((float)bytesRd / (float)fileInfo.st_size);
      //cout << "\r...  " << progress << "% complete (" << (bytesRd >> 10) << "kb read)   "  << flush;
      cout << progress << "% complete (" << (bytesRd >> 10) << "kb read)" << endl;

    //}

    buf[n] = '\0';

    //return file position to index of last delimiter (the end of the last regular structure), to get a bounded sample.
    lastDelim = seekLastPhraseDelimiter(buf,n);
    if(lastDelim > -1){

      buf[lastDelim] = '\0';

      //prevents infinite loop at end of file: we will continuously wind back to last delim if not checked.
      if(n == READ_SZ){
        lseek(inFile, lastDelim - n + 1, SEEK_CUR);
      }

      //a series of pipes/filters to clean up sample, and map it into our parse semantics
      normalizeText(buf,obuf);
      strncpy(buf, obuf.c_str(), BUFSIZE);
      buf[BUFSIZE-1] = '\0';
      filterSequence(buf,wordBuf);


      vecCt += updateVectorTable(buf,utime);
      cout << "in train(" << trainingFile << "). vecCt=" << vecCt << endl;
    }
    else{
      cout << "WARN no phrase delimiter found in buf, input discarded: >" << buf << "< delim index: " << lastDelim << " phraseDelims: " << phraseDelimiters << endl;
    }
  }
  close(inFile);

  //dbg
  i = 0;
  for(VectorTableIt it = vectorTable.begin(); i < 1000 && it != vectorTable.end(); i++, ++it){
    cout << it->first << "|listsz=" << it->second.size() << endl; 
  }
  cout << "Train(" << trainingFile << ") complete. vecCt=" << vecCt << " vecTable.size()=" << vectorTable.size() << endl;
  cin >> buf[0];
}
*/

bool kNN::isInt(const string& nstr)
{
  for(U32 i = 0; i < nstr.size(); i++){
    if(nstr[i] < 48 || nstr[i] > 57){
      return false;
    }
  }
  return true;
}

/*
  Allow 0-9, E, e, -, +, and .
*/
bool kNN::isDecimal(const string& nstr)
{
  for(int i = 0; i < nstr.size(); i++){
    if(!validDecimalChars[(int)nstr[i]]){
      return false;
    }
  }
  return true;
}

bool kNN::hasColon(const char buf[], int len)
{
  for(int i = 0; buf[i] != '\0' && i < len; i++){
    if(buf[i] == ':'){
      return true;
    }
  }
  return false;
}

//rebuilds the list of means from a file. Goofy name. Wahtever.
void kNN::recallMeansFromFile(void)
{
  int fd, ci;
  char buf[BUFSIZE];
  string s;

  clusterMeans.clear();
  meansIds.clear();
  idCounter = 0;

  fd = open(meansFile.c_str(), O_RDONLY);
  if(fd < 0){
    cout << "ERROR could not open file >" << meansFile << "< in recallMeansFromFile" << endl;
    return;
  }

  while(getLine(fd,buf,BUFSIZE) != NULL){
    if(hasColon(buf,BUFSIZE)){
      ci = findChar(buf,':',BUFSIZE);
      if(ci >= 0){
        buf[ci] = '\0'; //chop
        s = buf;
        idCounter++;
        clusterMeans[s] = idCounter;
        meansIds[idCounter] = s;
      }
    }
  }

/*
  cout << "the means from file " << meansFile << ":" << endl;
  for(map<string,U16>::iterator it = clusterMeans.begin(); it != clusterMeans.end(); ++it){
    cout << it->first << ":" << it->second << " -> " << meansIds[it->second] << endl;
  }
*/
}



/*
  Substantially slower than building the table from an in-memory word sequence. This, despite the fact
  that for the in-mem version, you have to calculate the mindist each time. However, its still faster than
  the i/o calls for each line, as here.
*/
void kNN::buildCoDistTableFromFile(const string& fname)
{
  int ifile;
  U32 ntokens, i;
  U16 k1, k2;
  U64 bytesRd;
  char buf[256];
  char* tokens[32];
  string tab = "\t";
  string w1, w2, w3;
  struct stat fileInfo;
  float progress;
  FloatType dist;
  CoDistInnerIt inner;

  cout << "clearing CoDistTable and building CoDistTable from file: " << fname << endl;
  if(!CoDistTable.empty()){
    cout << "WARNING CoDistTable not empty on call to buildCoDistTable, clearing table" << endl;
    CoDistTable.clear();
  }

  ifile = open(fname.c_str(), O_RDONLY);
  if(ifile < 0){
    cout << "ERROR could not open dbfile >" << fname << "< in buildCoDistTableFromFile" << endl;
    return;
  }
  if(fstat(ifile, &fileInfo)){
    cout << "ERROR fstat() call failed in testModel()" << endl;
    return;
  }

  i = 0; bytesRd = 0;
  while(getLine(ifile,buf,256)){
    bytesRd += strnlen(buf,256);
    ntokens = tokenize(tokens,buf,tab);
    if(ntokens == 3){
      w1 = tokens[0];
      w2 = tokens[1];
      w3 = tokens[2];
      if(isDecimal(w3)){
//      if(isInt(w3)){
        //dist = atoi(w3.c_str());
        dist = atof(w3.c_str());
        if((w1 != w2) && !CoDistTableHasKey(CoDistTable,w1,w2,inner)){  //check if key is already in table. left-hand check verifies no duplicate-word keys.
          //cout << "inserting: " << w1 << " " << w2 << " " << dist << endl;
          k1 = strToId(w1);
          k2 = strToId(w2);
          CoDistTable[k1][k2] = dist;
        }
        //else{
        //  cout << "WARN dupe key ignored in buildCoDistTable(): >" << w1 << "," << w2 << "<" << endl;
        //}
      }
      else{
        cout << "ERROR str not decimal-string >" << w3 << "< in buildCoDistTableFromFile()" << endl;
      }
    }
    else{
      cout << "ERROR nTokens=" << ntokens << " in buildCoDistTable." << endl;
    }

    i++;
    if((i %= 2000) == 0){
      progress = 101 * ((float)bytesRd / (float)fileInfo.st_size);
      //cout << "\r...  " << progress << "% complete (" << (bytesRd >> 10) << "kb read)   "  << flush;
      cout << "\r" << (int)progress << "% complete (" << (bytesRd >> 10) << "kb read)  CoDistTable.size()=" << CoDistTable.size() << "      " << flush;
    }
  }
  cout << "DONE building from file, CoDistTable.size()=" << CoDistTable.size() << endl;
}

/*
../../stopWordsLong.txt

*/
void kNN::buildStopWordTable(const string& stopWordFile)
{
  int i;
  string s;
  fstream stopfile(stopWordFile.c_str(), ios::in);

  //stopfile.open(stopWordFile.c_str(), ios::read);
  if(!stopfile){
    cout << "ERROR could not open file: " << stopWordFile << endl;
    return;
  }

  cout << "Stopwords:\n    ";
  i = 1;
  while(getline(stopfile,s)){  // same as: while (getline( myfile, line ).good())
    if(stopWords.count(s) == 0){
      stopWords.insert(s);
      cout << "  " << s;
    }
    if((i %= 14) == 0){
      cout << "\n  ";
    }
    i++;
  }
  cout << endl;  

  stopfile.close();
}

bool kNN::isStopWord(const char* word){
  string s = word;
  return (bool)stopWords.count(s);
}

bool kNN::isStopWord(const string& word){
  return stopWords.count(word);
}

void kNN::printScore(void) const
{
  cout << "~~Score: nPredictions=" << modelScore.nPredictions << " avgResultSize=" << modelScore.avgResultSize
       << " realAccuracy=" << modelScore.realAccuracy << " boolAccuracy=" << modelScore.boolAccuracy << endl; 

  cout << "~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~"
       << "\nnPredictions: " << modelScore.nPredictions << endl;
  if(modelScore.nPredictions > 0.0){ //div zero check
    cout << "realAccuracy: " << ((modelScore.realAccuracy / modelScore.nPredictions) * 100) << "%"
         << "\nboolAccuracy: " << ((modelScore.boolAccuracy / modelScore.nPredictions) * 100) << "%"
         << "\ntop10 accuracy:  " << ((modelScore.topN / modelScore.nPredictions) * 100) << "%"
         << "\navg predictive proximity:  " << ((modelScore.predictionProximity / modelScore.nPredictions) * 100) << "%"
         << "\nrecall:  " << ((modelScore.recall / modelScore.nPredictions) * 100) << "%"
         << "\navgWordLen:  " << (modelScore.avgWordLen / modelScore.topN) << " chars (pessimistic, only related to correct predictions in first 10 results)"
         << "\navgResultSz:  " << (modelScore.avgResultSize / modelScore.nPredictions) << endl;
  }
  else{
    cout << "realAccuracy: " << modelScore.realAccuracy << "%"
         << "\nboolAccuracy: " << modelScore.boolAccuracy << "%"
         << "\navgResultSz:  " << modelScore.avgResultSize << endl;
  }
  //avg res size / nPredictions to get avg
}

void kNN::clearScore(void)
{
  modelScore.avgResultSize = 0;
  modelScore.nPredictions = 0;
  modelScore.realAccuracy = 0;
  modelScore.boolAccuracy = 0;
  modelScore.avgWordLen = 0;
  modelScore.topN = 0;
  modelScore.recall = 0;
  modelScore.predictionProximity = 0;
}

//there are 45 files in the enronSent data set, each named as "enronSentxx" where xx = [00-44]
void kNN::testEnronParsed(const string& dir)
{
  string path;
  char suffix[4] = "00";
  char buf[128];
  
  for(int i = 0; i < 24; i++){
    sprintf(buf,"%s/enronsent%s",dir.c_str(),suffix);
    path = buf;
    cout << "In testEnronParsed, testing file: " << path << endl;
    testModel(path);
    printScore();

    //update the file suffix
    suffix[1]++;
    if((suffix[1] - '0') == 10){ //carry
      suffix[1] = '0';
      suffix[0]++;
    }
  }
  cout << "final results:" << endl;
  printScore();
}

/*
  Given a sequence of (pre-filtered) words extracted with this utime, build the context vectors
  and push them to the table.
*/
U32 kNN::updateVectorTable(vector<string>& wordVec, DateTime utime)
{
  U32 i, j, postLen;
  string preStr, m_postStr;
  char mbuf[1024];
  /*
  for(i = 0; i < len; i++){
    cout << i << ": " << wordList[i] << endl;
  }
  cout << "the wordlist" << endl;
  cin >> i;
  */

  cout << "building example-vector table, be patient..." << endl;

  /*
  cout << "\nvec:" << endl;
  //nTokens = 0, i = 0;
  for(vector<char*>::iterator it = wordList.begin(); it != wordList.end(); ++it){
    cout << *it << " ";
    //grams[i] = *it;
  }
  cin >> i;
  */
  /*
  grams[i] = NULL;
  cout << endl;
  cin >> i;
  */
  //end filter: now grams[] only contains non-stop words, in same order as previous
  for(i = 0; i < wordVec.size()-5; i++){
  //for(i = 0; i < wordList.size() - 5; i++){

    if(i % 50000 == 0){
      cout << "\r" << (100.0 * ((double)i / (double)wordVec.size())) << "% complete     " << flush;
    }

    //build query string of some number "n" previous context words.
    //preStr = wordVec[i];
    /*
    for(j = 1; j < NGRAMS_IN_QSTR; j++){
      preStr += " ";
      preStr += wordVec[j + i];
    }
    */
    j = 1; //for now, just use a single word as the key, instead of an n-gram key
    //cout << "prestr= >" << preStr << "<" << endl;
    //cout << "word[i/i+1] >" << wordVec[i] << "< >" << wordVec[i+1] << "<" << endl;

    //build post-string of next 3 words (context) after query
    postLen = (wordVec[j+i].size() + wordVec[j+i+1].size() + wordVec[j+i+2].size() + 2); // +2 for the spaces
    //postLen = (wordVec[j+i].size() + wordVec[j+i+1].size() + wordVec[j+i+2].size() + wordVec[j+i+3].size() + 3); // +3 for the spaces
    if(postLen < 1024){
      sprintf(mbuf,"%s %s %s",wordVec[j+i].c_str(),wordVec[j+i+1].c_str(),wordVec[j+i+2].c_str());
      //sprintf(mbuf,"%s %s %s %s",wordVec[j+i].c_str(),wordVec[j+i+1].c_str(),wordVec[j+i+2].c_str(),wordVec[j+i+3].c_str());
      mbuf[postLen] = '\0';
      //m_postStr = mbuf;
      //cout << "postStr: " << mbuf << endl;
      //build vector and insert into table
      attribVec vec;
      vec.count = 1;
      //vec.dt = utime;
      vec.postStr = mbuf;
      //cout << "pushing: " << preStr << "->" << vec.postStr << endl;
    //insertVector(wordVec[i],vec);
      vectorTable[wordVec[i]].push_back(vec);
    }
    else{
      cout << "WARN m_postStr too long, output skipped in updateVecTable" << endl;
    }

    preStr.clear();
    //m_postStr.clear();
  }

  cout << "\nconsolidating..." << endl;
  //consolidates vector counts in the table. Takes ~15 mins to complete
  consolidateExamples();
  cout << "consolidation completed, table size =" << vectorTable.size() << endl;

  string s = "../vecTable_";
  s += std::to_string(NGRAMS_IN_POSTSTR);
  s += "g_poststr.txt";
  putVectors(s);

  return i;
}

/*
  This is a utility specifically for a memory-wasting, but more time-efficient method of building the example table.
  The example table is designed to hold all vecotrs for some preceding word, say, "dog": "ate beef stew" "chased the cat" etc.
  Each vector has a count, but its more time efficient to simply push all example "dog" vectors, ignoring duplicates. This
  wastes memory, but is more time-efficient, since once all vectors have been pushed, you can just run through and and consolidate
  the duplicates.  This utility serves that purpose. It is what I call a "dumb" utility in that its context is very limited and specific.
*/
void kNN::consolidateExamples(void)
{
  U32 ct = 0;
  U32 i, j;
  VectorTableIt outer = vectorTable.begin();
  //VectorListIt inner, runner;
  VecList nonDupeList;

  //check each vector for duplicates, merging counts and deleting duplicates
  for(outer = vectorTable.begin(); outer != vectorTable.end(); ++outer){
    //cout << "enum size " << outer->second.size() << " key=" << outer->first << endl;
    //enumerate counts in this vector
    for(i = 0; i < outer->second.size(); i++){
      if(outer->second[i].count != 0){
        //the runner
        for(j = i + 1; j < outer->second.size(); j++){
          if(outer->second[i].postStr == outer->second[j].postStr){
            outer->second[j].count = 0;   //flags this example for deletion
            outer->second[i].count++;     //and increment its pair
            //cout << "runner count:" << runner->count << endl;
          }
        }
      }
      if(i % 400 == 0){
        cout << "\r" << (100.0 * (double)i / (double)outer->second.size()) << "% complete vec " << i << "   " << flush;
      }
    }
    //cout << "copy" << endl;
    //copy then delete all dupes
    for(i = 0; i < outer->second.size(); i++){
      if(outer->second[i].count > 0){
        nonDupeList.push_back(outer->second[i]);
      }
    }
    outer->second.clear();
    outer->second = nonDupeList;
    nonDupeList.clear();

    ct++;
    //cout << "\r" << (100.0 * (double)ct / (double)vectorTable.size()) << "% complete     " << flush;
    if(ct % 100 == 0){
      cout << "\r" << (100.0 * (double)ct / (double)vectorTable.size()) << "% complete     " << flush;
    }
  }
}

//small inner utility for saving a vector table to disk, to allow rebuilding from disk instead of recalculating the whole thing
//linux specific, because I'm too lazy to write the fstream version, and this function isn't much needed.
void kNN::putVectors(const string& fname)
{
  string ostring;
  VecTableIt it;
  int ofile, ct, i;
  set<string> examples;

  ofile = open(fname.c_str(),O_WRONLY);
  if(ofile < 0){
    cout << "ERROR open() returned " << ofile << " in putVectors() for file " << fname << endl;
    return;
  }

  ostring.reserve(32767);

  cout << "writing vectors to " << fname << endl;
  //format the file as: key\tpostStr|ct\postStr|ct.... where keys/tuple are tab-delimited, tuple vals are pipe-delimited
  for(ct = 0, it = vectorTable.begin(); it != vectorTable.end(); ++it){
    //ostring += it->first;
    for(i = 0; i < it->second.size(); i++){
      ostring = "\t";
      ostring += it->second[i].postStr;
      ostring += "|";
      ostring += std::to_string(it->second[i].count);
      examples.insert(ostring);
      ostring.clear();
    }

    //example strings built, now shoot them to a file
    ostring.clear();
    write(ofile, it->first.c_str(), it->first.length());  //write the query term
    for(set<string>::iterator it = examples.begin(); it != examples.end(); ++it){
      ostring += *it;
    }
    ostring += "\n";
    write(ofile, ostring.c_str(), ostring.length());  //write the query term
    ostring.clear();
    examples.clear();
  }

  close(ofile);
}

//rebuild from above output
void kNN::getVectors(const string& fname)
{
  U32 ct;
  int ifile, ntoks, i, n;
  char* keyVals[8];
  string dtabs = "\t";
  string dpipes = "|";
  string post, key;
  char buf[6000000];  //this is very dangerous. If rebuilding from a larger training set, large records like "the" "is" etc will easily exceed this size
  char* tokens[256000];

  ifile = open(fname.c_str(),O_RDONLY);

  cout << "building vector table from file..." << endl;

  //vectorTable.clear();

  ct = 0;
  while(getLine(ifile,buf,6000000)){
    //cout << "line" << endl;
    ntoks = tokenize(tokens,buf,dtabs);
    //cout << "tokenize" << endl;
    key = tokens[0];
    for(i = 1; i < ntoks; i++){
      //cout << "token=" << tokens[i] << endl;
      n = tokenize(keyVals,tokens[i],dpipes);
      if(n == 2){
        post = keyVals[0];
        AttributeVector vec = {post,(U16)atoi(keyVals[1])};
        vectorTable[key].push_back(vec);
      }
      else{
        cout << "ERROR wrong n toks " << n << " in second tokenize(): " << keyVals[0] << endl;
      }
    }

    ct++;
    if(ct % 500 == 0){
      cout << "\r" << ct << " lines read    " << flush;
    }
    //cout << "\r" << ct << " lines read    " << flush;
  }
  cout << "done building vectorTable, size=" << vectorTable.size() << endl;
}


void kNN::insertVector(const string& key, attribVec& vec)
{
  VecTableIt outer = vectorTable.find(key);
  bool found = false;

  vec.count = 1;

  //insert immediately and return if key is new
  if(outer == vectorTable.end()){
    vectorTable[key].push_back(vec);
    return;
  }
  
  //else, spool through the existing vectors looking for a current match on 3 words of post-string
  for(int i = 0; i < outer->second.size(); i++){
    if(outer->second[i].postStr == vec.postStr){
      outer->second[i].count++;
      found = true;
    }
  }

  if(!found){
    outer->second.push_back(vec);
  }
}


/*
  Given some testFile of text, read in the text and test the predictive accuracy of the kNN model.
  
  Some test metrics:
    -boolean accuracy: the number of exact hits, divided by the total number of words. An exact hit is when the predicted word == next word.
    -real accuracy: a real measure of accuracy, in which a the next word is scored by how deeply the matching word is nested (ranked) in the result set.
    -recall: the number of times the next word is in each result set, div number of words. This measures the overall quality of the kNN result set.

  Previous output:
    After training on the OAN corpus, and then also testing on the same corpus (for a base line, obviously this is a bad idea)
    the algorithm only achieved about 12.5/17.0% boolean/real accuracy after some 125000 predictions. The method was given
    an input word, get all examples for that word, and rank predictions based on the sum relatedness of the post-str words to the
    input word (in other words, the input word was the only context). These results were based on a significance measure -based
    distance function, using Poisson method_1 (see Bordag).  The method often overestimate significance for words which shared no
    co-occurrences (see Bordag), and also does not really satisfy the triangle inequality. For instance, my distance metric summed
    all the significance/Poisson1 based distances of some previous words, to get a value indicating the semantic strength (higher
    values being desirable, indicating more significance). However, I never really considered if the summming was valid, since
    the values don't share a linear relationship, so its not clear they are "summable".


    With improved weighted cosine distance:
    nPredictions: 40001
    realAccuracy: 47.4346%
    boolAccuracy: 31.9942%
    Clearly, the cosine method is working better. I think the big difference with the weighted cosine distance is a result of 
    it being closer to a similarity-based measure, as opposed to a significance-based measure, since it evaluates all transitive
    relations on words.  Mathematically, and most importantly, it gives a value ranging from 0 to 1, which is sort of a pesudo-probability
    for which it is likely more valid to sum over.

2/5:  (for this many predictions, results were convergent. Completing the remaining predictions would not alter the results)
params: 4 grams in post-string
~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
nPredictions: 49201
realAccuracy: 10.9638%
boolAccuracy: 5.73769%
top10 accuracy:  21.7618%
avg predictive proximity:  -nan%
recall:  75.7281%
avgWordLen: 

2/6 (convergent: for this many predictions the accuracy settled to 5%, no need to run all predictions)
progress: 75400/492054
~~Score: nPredictions=75401 avgResultSize=1.97201e+09 realAccuracy=7698.67 boolAccuracy=3780
~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
nPredictions: 75401
realAccuracy: 10.2103%
boolAccuracy: 5.0132%
top10 accuracy:  21.3313%
avg predictive proximity:  -nan%
recall:  75.2709%
avgWordLen:  2.96736 chars (pessimistic, only related to correct predictions in first 10 results)
avgResultSz:  26153.6





*/
void kNN::testModel(const string& testFile)
{
  int fd;
  //char c;
  U32 i;
  string s;
  vector<string> wordSequence;
  ResultList results;
  DateTime dt;

  //TODO: dont like how this foo uses an int-file descriptor, whereas the next foo() use char* fname. mixed patterns.
  fd = open(testFile.c_str(), O_RDONLY);
  if(fd < 0){
    cout << "ERROR could not open file in testModel: " << testFile << endl;
    return;
  }
  if(vectorTable.empty()){
    cout << "ERROR vectorTable empty in testModel()" << endl;
    return;
  }
  cout << "in testModel() vectorTable.size()=" << vectorTable.size() << endl;
/*
  dt = searchDateTime(fd);
  if(dt.hr == 255){
    cout << "WARN dt not found in file: " << testFile << endl;
  }
*/

  clearScore();
  textToWordSequence(testFile, wordSequence, false);

  //for each word frame, make a prediction
  for(i = 0; i < wordSequence.size() - (NGRAMS_IN_QSTR + NGRAMS_IN_POSTSTR) - 2; i++){ //minus 1, since we need to include the predicted word
    /*s = wordSequence[i];
    s+= " ":
    s += wordSequence[i+1]
    */

    //cout << "predict(" << wordSequence[i] << ",dt.yr=" << ((int)dt.year+1970) << ")" << endl;
    predict(wordSequence, i, dt, results);
    cout << "result.size()=" << results.size() << endl;
    //cin >> c;
    scorePrediction(wordSequence[i+1], results);
    results.clear();
    
    if(i % 100 == 0){
      cout << "progress: " << i << "/" << wordSequence.size() << endl;
      printScore();
    }
  }

  //finalizeScore();
  printScore();
}

/*
  Given some context words, tries to estimate the likeliest cluster we're currently in,
  as a raw value. This is just degenerate maximum likelihood estimation, so it probably
  shouldn't be a learning parameter. Instead, a cluster-likelihood should be used, where
  likelihood = sumDistance(context,mu) / sumDistance(context,all the mu's)

  Returns the id of the most likely current cluster, given 30 previous words of input.
*/
U16 kNN::estimateCurrentCluster(const vector<string>& context, int i)
{
  U16 maxId = 0;
  double sum, maxSum;
  ClusterMapIt it;

  maxSum = 0;
  //calculate the sum distance of context with each mean
  for(it = clusterMeans.begin(); it != clusterMeans.end(); ++it){
    sum = 0.0;
    for(int j = i; j > 0 && j > i - CLUSTER_RADIUS; j--){
      sum += getSimilarity(it->first,context[j]);
    }
    if(sum > maxSum){
      maxSum = sum;
      maxId = it->second;
    }
  }

  return maxId;
}

/*
  Given some current cluster estimate, get the likelihood that some vector
  of three words belongs to that cluster. The measure if given by vector's sumdist
  to the current cluster, divided by its sum distance to all other vectors.
  Basically queries the probability that this word group belongs to the current cluster
  instead of some other cluster.

  This is somewhat a degenerate form of simply assigning a raw sum-distance score
  to each vector, without any cluster data. Its kind of a statistical property that
  could just as well be factored or marginalzied out somehow. The cluster's are nice, but the same properties
  should be built into the sumDistance(word,context) value anyway. Splitting on cluster data
  fits more of a decision tree approach, while raw-sum distances fit more of a max-likelihood/bayesian
  approach. Maximum likelihood always fits language better, since multiple contexts occur
  simultaneously, so splitting on them would just be noisy.
*/
double kNN::getClusterLikelihood(U16 currentCluster, const vector<string>& context, int i)
{
  int j;
  double sum;
  double dist;
  ClusterMapIt it;

  //get the sum distance to all other clusters
  sum = 0.0;
  for(it = clusterMeans.begin(); it != clusterMeans.end(); ++it){
    for(j = i; j < 0 && j < i - CLUSTER_RADIUS; j--){
      sum += getSimilarity(it->first,context[j]);
    }
  }

  dist = 0.0;
  for(j = i; j < 0 && j < i - CLUSTER_RADIUS; j--){
    dist += getSimilarity(meansIds[currentCluster], context[j]);
  }

  if(sum > 0.0){
    return dist / sum; //div zero check
  }
  
  cout << "ERROR sum==0.0 in getClusterLikelihood()" << endl;
  return 0.0;
}

/*
  Evaluates and stores a real-valued score for the predictions. See definition for recall and accuracy.
  Accuracy is measured with a tapered tolerance: if the actual word is in the result set, its score is
  given by 1/i, where i is the index of the item in the list. This gives a rapidly-diminishing real-value score.

*/
void kNN::scorePrediction(const string& actual, ResultList& resultList)
{
  int i;
  bool found;
  long double rank;
  ResultListIt it = resultList.begin();

  modelScore.nPredictions++;
  modelScore.avgResultSize += resultList.size();

  if(resultList.empty()){
    return;
  }
  //cout << "in scorePrediction()" << endl;
  //update the boolean accuracy first. On average, this costs less than checking this w/in the loop
  if(strncmp(it->first->postStr.c_str(),actual.c_str(),actual.length()) == 0){
    modelScore.boolAccuracy++;
  }
  //cout << "here3" << endl;
  found = false;
  for(rank = 1.0; !found && it != resultList.end(); ++it, rank++){

    //note using strncmp() means doing a SUBSTRING comparison. so postStr="likelihood of the" and actual="like" will count as a hit! 
    if(strncmp(it->first->postStr.c_str(), actual.c_str(), actual.length()) == 0){
      modelScore.realAccuracy += ((long double)1.0 / rank);
      //cout << "HIT! >> " << actual << " | " << it->first->postStr << endl;
      found = true;
      modelScore.recall++;
      modelScore.predictionProximity += (it->second.semanticDist / resultList.begin()->second.semanticDist);
      if(rank <= 10){
        modelScore.topN++;
        modelScore.avgWordLen += (long double)actual.length();
        //modelScore.avgWordLen += ((long double)actual.length() / (long double)rank);
      }
    }

    /* //also check if prediction lies within three words
    for(i = 0; i < 5; i++){
    }
    */
  }

/*
  //dbg
  cout << "first ten, actual=" << actual << ": ";
  for(i = 0, it = resultList.begin(); i < 10 && it != resultList.end(); ++it, i++){
    cout << it->first->postStr << "**";
  }
  cout << endl;
  it = resultList.begin();
*/
}

/*
  Given two previous words context, try and predict the next word.
  Returns: a result list of references to vectors in the vector table, ranked by some score

Yay!
~~Score: nPredictions=10301 avgResultSize=2.73202e+08 realAccuracy=1356.27 boolAccuracy=902
~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
nPredictions: 10301
realAccuracy: 13.1664%
boolAccuracy: 8.75643%
top10 accuracy:  21.7843%
avg predictive proximity:  -nan%
recall:  76.8081%
avgWordLen:  3.27941 chars (pessimistic, only related to correct predictions in first 10 results)
avgResultSz:  26521.9








*/
void kNN::predict(vector<string>& wordSeq, int wordIndex, const DateTime& dt, ResultList& resultList)
{
  U16 currentCluster;

  //logic exception
  if(vectorTable.empty()){
    cout << "ERROR table of example vectors empty in predictWord" << endl;
    return;
  }
  //cout << "in predict()" << endl;
  //given some n previous words of context, look up all example vectors to predict next word
 
  //this could also be done periodically, instead of every prediction and passed as a parameter. for research it needs to be done every time, for precise results.
  currentCluster = estimateCurrentCluster(wordSeq,wordIndex);
/*  if(currentCluster > 0){
    cout << "current cluster: " << currentCluster << ":" << meansIds[currentCluster] << endl;
  }
*/
  //cout << "getting examples" << endl;
  //get the raw, unsorted/unscored result set
  getExamples(wordSeq[wordIndex], resultList);
  //cout <<"ranking " << resultList.size() << " examples" << endl;
  //score and sort the results based on distance
  rankResults(wordSeq, wordIndex, resultList, currentCluster);
}

//utility for building the raw result set of possible example vectors mapped to a query
void kNN::getExamples(const string& w1, ResultList& resultList)
{
  VectorTableIt outer;
  VectorListIt inner;
 
  if(!resultList.empty()){ //clear the result list if not empty
    resultList.clear();
  }

  //builds the raw, unsorted/unscored result list
  //q = w1; q += " "; q += w2;
  //cout << "w1 is: " << w1 << endl;
  outer = vectorTable.find(w1);
  if(outer != vectorTable.end()){
    for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      Delta dt;
      pair<VecListIt,Delta> p(inner,dt);
      resultList.push_front( p );
    }
  }
  else{
    cout << "query string >" << w1 << "< not found in example vector set! attempting synonym..." << endl;
    //its somewhat unlikely a query string not in the examples will have a synonym in the similarity models, but try anyway
    //getting a set of synonyms can be far too expensive
    U32 sy = getSynonym(w1);
    if(sy != 0){
      outer = vectorTable.find(idToStr(sy));
      if(outer != vectorTable.end()){
        for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
          Delta dt;
          pair<VecListIt,Delta> p(inner,dt);
          resultList.push_front( p );
        }
      }
    }
  }
  /*  //on hold. this was for two-word, reversible query strings
  //q = w2; q += " "; q += w1;
  cout << "q is: " << q << endl;
  outer = vectorTable.find(q);
  if(outer != vectorTable.end()){
    for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      Delta dt;
      pair<VecListIt,Delta> p(inner,dt);
      resultList.push_front(p);
    }
  }
  */
}

//just gets the max most likely synonym for a word
U32 kNN::getSynonym(const string& q)
{
  FloatType max;
  CoDistInnerIt maxIt;
  U32 syn = 0, id;

  max = -1;
  id = strToId(q);
  CoDistOuterIt outer = CoDistTable.find(id);
  if(outer != CoDistTable.end()){
    for(CoDistInnerIt inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      if(max < inner->second){
        max = inner->second;
        maxIt = inner;
      }
    }
  }

  if(max > 0){
    syn = maxIt->first;
  }
 
  return syn;
}

//sort descending
bool bySynonymity(const pair<U32,FloatType>& left, const pair<U32,FloatType>& right)
{
  return left.second > right.second;
}

/*
  The Similarity-model (the cosine-based word similarity matrix) can be very useful for synonym analysis.
  This function looks up a word in the model and attempts to find its top-k synonyms.
*/
int kNN::getSynonyms(const string& q, list<pair<U32,FloatType> >& synonyms, int k)
{
  int i;
  U32 id;
  list<pair<U32,FloatType> > temp;
  list<pair<U32,FloatType> >::iterator it;

  id = strToId(q);
  CoDistOuterIt outer = CoDistTable.find(id);
  if(outer != CoDistTable.end()){
    for(CoDistInnerIt inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      pair<U32,FloatType> p(inner->first, inner->second);
      temp.push_back(p);
    }
    cout << "sorting synonyms, list.size()=" << temp.size() << endl;
    temp.sort(bySynonymity);
    it = temp.begin();
    for(i = 0; i < k && it != temp.end(); ++it, i++){
      synonyms.push_back(*it);
    }
    temp.clear();
  }

/*
  if(i > 0){
    cout << "top synonym for >" << q << "< is: " << idToStr(synonyms.begin()->first) << endl;
  }
*/
  return i;
}

/*
  Based on some context of previous k words, try to estimate the current cluster we're in.
  Clearly this is very stateful: CoDist, clusters, and all other must be in-memory and synce'd;
  ideally, these would be based off a much larger sample of language than just the OANC.
*/
U16 kNN::estimateClusterNo(const string& postStr)
{
  int i, j;
  FloatType max, sum;
  U16 maxId; //the id of the cluster with max likelihood (least distance to current context
  string t[3];
  
  //find indices of spaces in postStr words
  i = postStr.find(' ',0);
  j = postStr.find(' ',i+1);
  t[0] = postStr.substr(0, i);  //right arg is length of substr, starting from left arg
  t[1] = postStr.substr(i+1,(j-i-1));
  t[2] = postStr.substr(j+1);

  max = -1.0; maxId = 0;
  for(map<string,U16>::iterator it = clusterMeans.begin(); it != clusterMeans.end(); ++it){
    sum = 0.0;
    for(i = 0; i < 3; i++){
      sum += getSimilarity(t[i],it->first);
    }

    if(max < sum){
      max = sum;
      maxId = it->second;
    }
  }

  return maxId;
}

/*
  A depth-bounded merge, for giving back a pseudo-k-nearest neighbor output.
*/
void kNN::mergeTopKDuplicates(ResultList& sortedResults)
{
  int i, ct;
  ResultListIt outer, inner, del;
  string s;
  FloatType sum;

  //cout << "merging..." << endl;
  for(ct = 0, outer = sortedResults.begin(); ct < K_NEIGHBORS && outer != sortedResults.end(); ct++, ++outer){
    i = outer->first->postStr.find(' ',0);
    s = outer->first->postStr.substr(0, i);
    sum = outer->second.semanticDist;
    inner = outer;
    for(++inner; inner != sortedResults.end(); ++inner){
      if(strncmp(inner->first->postStr.c_str(),s.c_str(),s.length()) == 0){
        sum += inner->second.semanticDist;
        inner->second.semanticDist = 0;  //effectively deletes this entry, dropping it to the end on next sort
        //del = inner;
        //inner++;
        //sortedResults.remove(del);
        //cout << "inner" << endl;
      }
    }
    //cout << "outer" << endl;
    outer->second.semanticDist = sum;
  }
}


/*
  The result list contains vectors with expected values, where the three words in the post-string
  define the uniqueness of each vector. However, some vectors may begin with the same word, so 
  it is valid to sum the expected values for these vectors, to help gives these words a better shot.

  Only look at the top k or so results, to avoid going through the whole list.
*/
void kNN::mergeDuplicates(ResultList& sortedResults)
{
  int i;
  ResultListIt outer, inner, del;
  string s;
  FloatType sum;

  //cout << "merging..." << endl;
  for(outer = sortedResults.begin(); outer != sortedResults.end(); ++outer){
    i = outer->first->postStr.find(' ',0);
    s = outer->first->postStr.substr(0,i);
    sum = outer->second.semanticDist;

    //beginning with all results after this one, sum their distance if the first word (the prediction) is the same,
    //then set that instance's dist to 0, to effectively send it to the end of the list after sorting.
    inner = outer;
    for(++inner; inner != sortedResults.end(); ++inner){
      if(strncmp(inner->first->postStr.c_str(),s.c_str(),s.length()) == 0){
        sum += inner->second.semanticDist;
        inner->second.semanticDist = 0;  //effectively deletes this entry, dropping it to the end on next sort
        //del = inner;
        //inner++;
        //sortedResults.remove(del);
        //cout << "inner" << endl;
      }
    }
    //cout << "outer" << endl;
    outer->second.semanticDist = sum;
  }
}

/*
  This version is intended to calculate the distance of a vector (three words) to some cluster/mean.
  Typical usage is that we estimate the current cluster mean (some context, like "politics" or "sports),
  then compare the vector's overall distance to that cluster, such that we can compare it to the cluster
  distance of other such vectors.

  Note: Remember that a hidden parameter of this function is the clustering method used, and the significance/similarity
  method used to drive the clustering algorithm which developed our clusters.
*/
FloatType kNN::getClusterDistance(U16 clusterNo, const string& postStr)
{
  FloatType sum = 0.0;
  ClusterIdMapIt it = meansIds.find(clusterNo);

  //exception case: if clusterNo not found, this may be some Nil cluster. assume no relation with it.
  if(it == meansIds.end()){
    return 0.0;
  }

  return getSumDistance(it->second,postStr);
}




/*
  This is the core driver of the whole k-nearest neighbor algorithm for words.  Given
  two previous context words, and a populated result set, we iterate over the examples
  and find the result-list words "nearest" to the context words.
  
  Currently, the only parameters to the distance function are datetime (year, month, day),
  and the semantic relatedness between the context words (w1, w2) and the predicted words.
  The latter seems like a much better way to assess the "nearness" of word sets.  

  Conditioning context with 3 previous words showed lower accuracy: 8-real, 4-bool (%)
  Conditioning with only the one seed word gave highest accuracy so far: 19-real, 15-bool (%):
    nPredictions: 26101
    realAccuracy: 19.4793%
    boolAccuracy: 15.8538%
    avgResultSz:  523.232
  Conditioning with diminishing value of dist/i (where i=0 is seed, i = 3 is fourth prev word) 
  gave about 19.45/15.82 real/bool accuracy for 25k predictions.

  The above results represent overfitting, since the training data was used as the test data.
  Using a different test file, a subset of the enronParsed corpus, the results were:
    --conditioning context--
    one word:
    three words:


context_radius = 15
w/out cluster input:
nPredictions: 5101
realAccuracy: 8.02766%
boolAccuracy: 5.44991%
top10 accuracy:  13.1151%
avgResultSz:  459.078

w/cluster input does better, but not by much (could be considered noise):
nPredictions: 5101
realAccuracy: 8.2644%
boolAccuracy: 5.76358%
top10 accuracy:  13.0955%
avgResultSz:  459.078

k-nearest unfiltered version (low error experiment)
  ~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
  nPredictions: 1601
  realAccuracy: 10.7731%
  boolAccuracy: 6.80824%
  top10 accuracy:  18.6134%
  avgResultSz:  23877.5

12/6 results, using postStr of 3 words:
  ~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
  nPredictions: 10801
  realAccuracy: 13.2541%
  boolAccuracy: 8.79548%
  top10 accuracy:  21.9239%
  avg predictive proximity:  -nan%
  recall:  76.8077%
  avgWordLen:  3.26858 chars (pessimistic, only related to correct predictions in first 10 results)
  avgResultSz:  26511.4
  ...
  ~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
  nPredictions: 31701
  realAccuracy: 12.7914%
  boolAccuracy: 8.41298%
  top10 accuracy:  21.3905%
  avg predictive proximity:  -nan%
  recall:  75.6758%
  avgWordLen:  3.24082 chars (pessimistic, only related to correct predictions in first 10 results)
  avgResultSz:  26539.5


  Small set of 4-word post string predictions, appears to be doing worse than 3-word vectors
  ~~~~~~~~~~~~~~~~~Results~~~~~~~~~~~~~~~~~
  nPredictions: 2901
  realAccuracy: 11.3108%
  boolAccuracy: 6.96312%
  top10 accuracy:  19.8552%
  avg predictive proximity:  -nan%
  recall:  77.0079%
  avgWordLen:  3.24653 chars (pessimistic, only related to correct predictions in first 10 results)
  avgResultSz:  28487.9




The above shows a sturdy estimate of the efficacy of this kNN method.  Parameters were
clusterDist=15, contextRadius=15 (handpicked, based on previous accuracy). There is some
potential "n-gram creep" with this approach, since the previous word of input gives our
possible vectors, over which we sum the cluster/semantic distances. However, notice that
this approach breaks the linear predicting model of n-grams, since the summing step divulges
non-linear relationships between words. Note that the above results are somewhat pessimistic,
since we evalaute based on exact word-matches. Also, there is a hidden parameter in terms of
which semantic-distance model to use; in this case, the weighted-cosine distance was used,
which qualitatively measures the number of shared relationships between word1 and word2, which
is a very good way to estimate subject/topic relationships which are missed by n-gram models.
Another motivation is not purely boosting accuracy, but boosting ksr: if our predictions
are weaker, but the average word length of correct predictions is longer, the system
may have greater expected value. 

Another motivation for the approach of this algorithm is its ability to predict several words,
based on some previous input, such that predictions are phrases, rather than just words.

Also note that example vectors are merged based on if their postStr strings are equal. This
means they must match on the next three words, which is fairly strict. Need to retest
by merging vectors for which only the first word is the same, not all of the next 3.




TODO: re-test using a model which merges vectors for which the next word is the same, OR, without
merging the vectors in the model, at least assigning the same count value across vectors for which the next
word is the same. Just note that the postStr==postStr constraint is likely too strong, and find ways to generalize the behavior more.



*/
void kNN::rankResults(vector<string>& context, int wordIndex, ResultList& resultList, U16 currentCluster)
{
  //U32 sumDist;  //the sum semantic distance (avg mn distance) between {w1,w2} in the query and {w1-w5} in each result
  int i;
  FloatType normal;
  FloatType sumClusterDist; //sum distance of all the result vectors to the current cluster. this is a normalization
  //constant for deriving probability distributions: the probability that a given vector is in the current cluster.
  ResultListIt it;

  //sum all vector frequencies to get probabilities for each vector, given its occurrence
  //this can often be factored out, if for instance every vector.similarity is divided by it
  normal = 0.0;
  for(it = resultList.begin(); it != resultList.end(); ++it){
    normal += it->first->count;
  }
  //cout << "norm=" << normal << endl;

  //get the sum cluster distance (normalization constant) over all vectors in results
  //note this is somewhat degenerate. If we sum over sum semantic distance between context and Vi (a vector),
  //that gives a continuous value for the current context. The hypothesis is that also estimating the cluster
  //distance may also tighten up context, and elevate results with stronger associations with the current cluster.
  sumClusterDist = 0.0;
  for(it = resultList.begin(); it != resultList.end(); ++it){
    sumClusterDist += getClusterDistance(currentCluster, it->first->postStr) * (FloatType)it->first->count;
  }
  //cout << "sumClusterDist: " << sumClusterDist << endl;


  //cout << "in rank()" << endl;
  //for every result, assign deltas to each attribute
  for(it = resultList.begin(); it != resultList.end(); ++it){
    //get the raw, continuous distance between this vector and some measure of the current context.
    //remember this value is an expected value, so use it consistently (eg, not as a probability 0.0-1.0)
//    it->second.semanticDist = getSumDistance(context[wordIndex], it->first->postStr) * ((long double)it->first->count / normal);

    //estimate the distance of this vector from the current estimated cluster (topic)
//    it->second.semanticDist += (getClusterDistance(currentCluster, it->first->postStr) / sumClusterDist);


    it->second.semanticDist = (getSumDistance(context[wordIndex], it->first->postStr) * (FloatType)it->first->count) / normal; // * ((long double)it->first->count / normal);
    it->second.semanticDist += (getClusterDistance(currentCluster, it->first->postStr) * (FloatType)it->first->count) / sumClusterDist; //ignores sumClusterDist...
    //it->second.semanticDist *= ((long double)it->first->count / normal);
    //accounting for frequency noticeable improves things
    //it->second.semanticDist *= (long double)it->first->count;



    //it->second.clusterNumber = estimateClusterNo(it->first->postStr);
    //it->second.clusterLikelihood = estimateClusterNo(context,wordIndex);
    //cout << "estimated clusterno: " << it->second.clusteuprNumber << endl;
/*
    for(i = wordIndex-1; i > 0 && i > (wordIndex - 3); i--){
      it->second.semanticDist += (getSumDistance(context[i], it->first->postStr) / (long double)i);
    }
*/
    //it->second.yearDist = it->second.monthDist = it->second.dayDist = it->second.hrDist = 255;
/* //ignore datetime for now. OANC dataset does not have datetimes for each communication
    //or, sort by datetime distance for more of a timeframe-oriented distance
    if(curDT.hr != 255){ //flag for UNKNOWN curtime
      curDT.diffDateTimes(it->second, curDT, it->first->dt);
    }
    else{
      it->second.yearDist = it->second.monthDist = it->second.dayDist = it->second.hrDist = 255;
    }
*/
  }

  /*
    results are about 3-5% lower without merging. which is a good thing, and strongly indicates the benefit of merging
    It may result in some variant of k-nearest neighbors, but it is no longer, strictly speaking, k-nearest neighbors.

  */

  //mergeDuplicates doesn't require a sorted list, only sorts it afterward since it processes the entire list

  //NOT NEEDED IF VECTOR MODEL IS COUNT-MERGED
  //mergeDuplicates(resultList);  //combine the expected values for repeated words. this is equivalent to getting the top nearest neighbors

  //cout << "sorting" << endl;
  resultList.sort(byNearestWords);

/*
  //mergeTopKDuplicates requires pre-sorting input, then sorting the output again
  resultList.sort(byNearestWords);
  mergeTopKDuplicates(resultList);
  resultList.sort(byNearestWords);
*/


/*
  //dbg
  cout << "given context: ";
  for(int i = 8; i > 0 && i > context.size(); i--){
    cout << context[wordIndex - i] << " ";
  }
  cout << endl;
  for(ResultListIt it = resultList.begin(); it != resultList.end(); ++it){
    cout << ">> " << it->first->postStr << " | " << it->second.semanticDist << endl;
  }
  cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
*/
}

/*
  PRecondition: neither datetime struct can be 255
*/
void DateTime::diffDateTimes(Delta& d, const DateTime& curDT, const DateTime& srcDT)
{
/*
  //get the hour delta
  if(curDT.hr > srcDT.hr)
    d.hrDist = curDT.hr - srcDT.hr;
  else
    d.hrDist = srcDT.hr - curDT.hr;

  //get the day delta
  if(curDT.day > srcDT.day)
    d.dayDist = curDT.day - srcDT.day;
  else
    d.dayDist = srcDT.day - curDT.day;

  //get the month delta
  if(curDT.month > srcDT.month)
    d.monthDist = curDT.month - srcDT.month;
  else
    d.monthDist = srcDT.month - curDT.month;

  //get the year delta
  if(curDT.year > srcDT.year)
    d.yearDist = curDT.year - srcDT.year;
  else
    d.yearDist = srcDT.year - curDT.year;
*/
}

/*
  Given some words of input, w1 and w2, and a set of words ww1-wwk in targets, sum
  the distances between each word. Shorter distances show more favorable
  relationships ("distance") between words.
*/
FloatType kNN::getSumDistance(const string& w1, const string& targets)
{
  U32 i, j, k;
  FloatType sum, min;
  //FloatType d[NGRAMS_IN_POSTSTR];
  string t[NGRAMS_IN_POSTSTR];
  
  //find indices of spaces in postStr words
  i = targets.find(' ',0);
  j = targets.find(' ',i+1);
//k = targets.find(' ',j+1);
  //t[0] = targets.substr(0, i);  //right arg is length of substr, starting from left arg
  //t[1] = targets.substr(i+1,(j-i-1));
  //t[2] = targets.substr(j+1);
  t[0] = targets.substr(0, i);  //right arg is length of substr, starting from left arg
  t[1] = targets.substr(i+1,(j-i-1));
  t[2] = targets.substr(j+1,k-j-1);
//t[3] = targets.substr(k+1);


  //cout << "targets: >" << t1 << "< >" << t2 << "< >" << t3 << "<" << endl;
  //cin >> sp1;getSumDi
  //min = 9999.0;
  sum = 0.0;
  for(i = 0; i < NGRAMS_IN_POSTSTR; i++){
    //d[i] = getSimilarity(w1,t[i]);
    //if(d[i] < min)
    //  min = d[i];
    sum += getSimilarity(w1,t[i]);
  }

  //NO DISTANCES FOUND! What to return???
/*  if(min == 9999){
    //cout << "returning MAX_SUM_WORD_DIST" << endl;
    return 0;
  }
*/
  
  /*cout << "calc'ing dist" << endl;
  sum = 0.0;
  for(i = 0; i < NGRAMS_IN_POSTSTR; i++){
    sum += d[i];
    if(d[i] > 0)
      sum += d[i];
    else
      sum += min;

  }
*/
  return sum;
}

/*
  Simple lookup.
  Same pattern as CoDistTableHasKey(), except we return the actual key-value.
  
  Returns distance between words or zero if two words not in table. Zero is returned in the
  not-found case (instead of say, -1), since this function will can be called iteratively to sum the
  distance between sets of words instead of just two words.

  This function has very important pre-conditions. There's a lot of state in the model building.
  The wordIndexTable must exist such that every w1 and w2 has a value in the table, which isn't
  true after the table is pruned. Also the numwords variable must be initialized with the correct
  value, and its not clear what this ought to be.

  Since the codist matrix is so sparse, when testing on real data, it will often be the case that we look up
  a word that is not in our codist models. Even at 8-10000 unique words, however, the models will still be sufficient
  to give a general estimate of most normal language, since the models contain most of the most common words.

*/
FloatType kNN::getSimilarity(const string& w1, const string& w2)
{
  U32 k1, k2;
  CoDistOuterIt outer;
  CoDistInnerIt inner;
  
  k1 = strToId(w1);
  k2 = strToId(w2);

  if(DBG){
    if(k1 == 0){
      cout << "ERROR k1=" << k1 << " for key1=" << w1 << " in updateCoDistTable" << endl;  
      return 0.0;
    }
    if(k2 == 0){
      cout << "ERROR k2=" << k2 << " for key2=" << w2 << " in updateCoDistTable" << endl;
      return 0.0;
    }
  }

  if(CoDistTable.empty()){
    cout << "ERROR CoDistTable empty in getAssociativity()" << endl;
    return 0.0;
  }

  outer = CoDistTable.find(k1);
  if(outer != CoDistTable.end()){
    inner = outer->second.find(k2);
    if(inner != outer->second.end()){
      return inner->second; 
    }
  }
  outer = CoDistTable.find(k2);
  if(outer != CoDistTable.end()){
    inner = outer->second.find(k1);
    if(inner != outer->second.end()){
      return inner->second; 
    }
  }

  //we know that the vocabulary and the codist table are aligned. however, many words share no co-occurrences,
  //so its still possible to not find them in the table. Hence this line.
  // *not consistent with cosine measures*
  //return (((long double)wordIndexTable[w1].second.size() * (long double)wordIndexTable[w2].second.size()) / (long double)this->numwords);

  //cout << "WARN getSimilarity returning 0.0, line should not be hit! w1=" << w1 << " w2=" << w2 << endl;
  return 0.0;
}


/*
  Same pattern as CoDistTableHasKey(), except we return the actual key-value.
  
  Returns distance between words or zero if two words not in table. Zero is returned in the
  not-found case (instead of say, -1), since this function will can be called iteratively to sum the
  distance between sets of words instead of just two words.

  PRECONDITIONS: wordIndexTable, CoDistTable, and numwords must all exist

*/
FloatType kNN::calculateSignificance(const U32 k1, const U32 k2)
{
  string s1, s2;
  FloatType lambda, n, nA, nB, nAB; //n_a is number of word a's, n_b number word b's, and n_ab is number of co-occurrences of a and b
  CoDistOuterIt outer;
  CoDistInnerIt inner;

  s1 = idToStr(k1);
  if(s1 == "NIL"){
    cout << "ERROR s1==NIL for k1=" << k1 << " in calculateSignificance()" << endl;
    return 1.0;
  }
  s2 = idToStr(k2);
  if(s2 == "NIL"){
    cout << "ERROR s1==NIL for k1=" << k2 << " in calculateSignificance()" << endl;
    return 1.0;
  }

/*  //error checking moved to a higher level, since this is a high-frequency function
  if(numwords <= 0){
    cout << "ERROR CoDistTable empty in getSimilarity()" << endl;
    return 0.0;
  }
  if(wordIndexTable.empty()){
    cout << "ERROR wordIndexTable empty in getSimilarity()" << endl;
    return 0.0;
  }
  if(CoDistTable.empty()){
    cout << "ERROR CoDistTable empty in getSimilarity()" << endl;
    return 0.0;
  }
*/

  // n, nA, and nB must/will have non-zero counts. nAB may have zero counts, however.
  /// error cases if any of these results are zero: 
  n = (FloatType)this->numwords;
  nA = wordIndexTable[s1].second.size();
  nB = wordIndexTable[s2].second.size();
  nAB = 0.0; // get the value of nAB below, which is often zero since the co-occurrence matrix is so sparse. If not found, nAB remains 0.0

  outer = CoDistTable.find(k1);
  if(outer != CoDistTable.end()){ //look in first key order: key = word1+word2
    inner = outer->second.find(k2);
    if(inner != outer->second.end()){
      nAB = inner->second; 
    }
  }
  else{                           //not found, so look in reverse order: key = word2+word1
    outer = CoDistTable.find(k2);
    if(outer != CoDistTable.end()){
      inner = outer->second.find(k1);
      if(inner != outer->second.end()){
        nAB = inner->second;
      }
    }
  }

  if(nA == 0)
    cout << "ERROR nA =" << nA << " in calculateSignificance()" << endl;
  if(nB == 0)
    cout << "ERROR nB =" << nB << " in calculateSignificance()" << endl;

  lambda = 0.0;
  //lambda = methodLogLikelihood(n,nA,nB,nAB);
  //lambda = method_Poisson1(n,nA,nB,nAB);
  if(nAB > 1.0){  //poisson2 returns 0.0 if nAB is 0, so no need to call. Additionally, there is a defect with the approx. for nAB = 1.0
    lambda = method_Poisson2(n,nA,nB,nAB);
  }

  //dbg
  if(lambda <= 0){
    cout << "n/nA/nB/nAB/lambda is: " << n << " " << nA << " " << nB << " " << nAB << " " << lambda << endl;
  }

  //cout << "n/nA/nB/nAB/lambda is: " << n << " " << nA << " " << nB << " " << nAB << " " << lambda << endl;
  //cin >> lambda;
  
  return lambda;
}


/*
  CALLER IS RESPONSIBLE FOR ERROR CHECKING PARAMS (eg, no zero div, etc)

  A Poisson approximation. From Bordag expression (6)

  letting lambda = nA*nB / n
  sigps1 (A, B) ≈ nAB (ln nAB − ln λ − 1) + 1/2 ln(2pi nAB) + λ
*/

FloatType kNN::method_Poisson1(FloatType n, FloatType nA, FloatType nB, FloatType nAB)
{
  FloatType lambda = (nA * nB) / n;  //no div zero check

  if(nAB > 0.0){
    //stirling approximation of ln(nAB!)
    lambda = nAB * (log(nAB) - log(lambda) - 1.0) + 0.5 * log(2.0 * MATH_PI * nAB) + lambda;
  }
  //else: return (nA * nB) / n

  return lambda;
}

/*
  From Bordag. Systematically overrates co-occurrence, which Bordag reports as beneficial to similarity measures.

  sigps2 (A, B) ≈ nAB (ln nAB − ln λ − 1)

  Obvious difference from the other measure is that this one will return zero if a and b share no co-occurrences,
  whereas the previous version returned lambda.
*/
FloatType kNN::method_Poisson2(FloatType n, FloatType nA, FloatType nB, FloatType nAB)
{
  FloatType sig = 0.0;

  if(nAB > 0.0){
    //stirling approximation of ln(nAB!)
    sig = nAB * (log(nAB) - log((nA * nB) / n) - 1.0);
  }

  //there is a defect with the above approximation if nAB == 1 or 2, for which sig is typically some value slightly less than 0.0
  if(sig < 0.0){
    sig = 0.0;
  }

  return sig;
}



/*
  method (5) from A Comparison of Co-occurrence and Similarity Measures as Simulations of Context by Bordag
  The calculations below are zero-protected version of this expression (see Bordag):
  lambda = n*log(n) - nA*log(nA) - nB*log(nB) + nAB*log(nAB) + (n - nA - nB + nAB )*log(n - nA - nB + nAB);
  lambda += (nA - nAB)*log(nA - nAB) + (nB - nAB)*log(nB - nAB) - (n - nA )*log(n - nA) - (n - nB)*log(n - nB );
*/
FloatType kNN::method_LogLikelihood(FloatType n, FloatType nA, FloatType nB, FloatType nAB)
{
  FloatType lambda = n*log10(n); // "> 1.5" prevents log(1), log(1.00...01), log(0), log(-f), etc.
  if(nA > 1.5){   // 1.5 is chosen to since nA, nB, nAB are integers, but as floats, they need a small buffer
    lambda -= nA*log10(nA);  // so we don't run into errors or comparisons like 1.000___01 == 1.000___00 == true
  }
  if(nB > 1.5){
    lambda -= nB*log10(nB);
  }
  if(nAB > 1.5){
    lambda += nAB*log10(nAB);
  }
  lambda += (n - nA - nB + nAB )*log10(n - nA - nB + nAB);
  //this is valid as nA and nB are lower-bounded by nAB. this check prevents both nA-nAB==0, and nA-nAB==1 (since log(1) is zero)
  if(nA > (nAB + 1.5)){
    lambda += (nA - nAB)*log10(nA - nAB);
  }
  if(nB > (nAB + 1.5)){
    lambda += (nB - nAB)*log10(nB - nAB);
  }
  lambda -= ((n - nA)*log10(n - nA));
  lambda -= ((n - nB)*log10(n - nB));

/*
  //method (5) from A Comparison of Co-occurrence and Similarity Measures as Simulations of Context by Bordag
  if(nAB > 0.0 && nA ){
    lambda = n*log(n) - nA*log(nA) - nB*log(nB) + nAB*log(nAB) + (n - nA - nB + nAB )*log(n - nA - nB + nAB);
    lambda += (nA - nAB)*log(nA - nAB) + (nB - nAB)*log(nB - nAB) - (n - nA )*log(n - nA) - (n - nB)*log(n - nB );
  }
  else{ //dropped nAB and log(nAB) terms from above summation/expression
    lambda = n*log(n) - nA*log(nA) - nB*log(nB) + (n - nA - nB)*log(n - nA - nB);
    lambda += nA*log(nA) + nB*log(nB) - (n - nA)*log(n - nA) - (n - nB)*log(n - nB );
  }
  //cout << "lambda is: " << lambda << endl;
*/


  if(nAB < ((nA*nB)/n)){
    lambda = -2.0*log10(lambda);
  }
  else{
    lambda = 2.0*log10(lambda);
  }

  return lambda;
}


/*
//old raw co-occurrence count version: returns n-times word a and b co-occur in some corpus, within some radius (a parameter like 25 words)
U32 kNN::getSimilarity(const string& w1, const string& w2)
{
  long double n_a, n_b, n_ab; //n_a is number of word a's, n_b number word b's, and n_ab is number of co-occurrences of a and b

  unordered_map<string, unordered_map<string,U32> >::iterator outer;
  CoDistInnerIt inner;

  if(CoDistTable.empty()){
    cout << "ERROR CoDistTable empty in getSimilarity()" << endl;
    return 0;
  }

  outer = CoDistTable.find(w1);
  if(outer != CoDistTable.end()){
    inner = outer->second.find(w2);
    if(inner != outer->second.end()){
      return inner->second; 
    }
  }
  outer = CoDistTable.find(w2);
  if(outer != CoDistTable.end()){
    inner = outer->second.find(w1);
    if(inner != outer->second.end()){
      return inner->second; 
    }
  }

  return 0;  
}
*/


/*
  Converts a datetime string "11/22/2222 22:22 PM" to a U32 value.
  byte ordering: hr|day|month|years_since_1970
  
  Precondition: input string is in fact some datetime string, formatted as "11/22/2222 22:22 PM"
*/
DateTime kNN::strToDateTime(char dtstr[])
{
  DateTime ret = {255,255,255,255};
  //U8 temp = 0;
  string s = " ";
  int nTokens;
  char* tokens[8];
  char* date[8];
  //char* time[8];

  //first split on whitespace to get tokens [11/22/1222, 05:40, PM]
  nTokens = tokenize(tokens,dtstr,s);
  if(nTokens != 3){
    //cout << "WARN wrong num tokens in strToDateTime" << endl;
    return ret;
  }

  //get the date tokens
  s = "/";
  nTokens = tokenize(date,tokens[0],s);
  if(nTokens != 3){
    cout << "error wrong num date tokens >" << nTokens << "< in strToDateTime" << endl;
    return ret;
  }
  //get the day
  ret.day = (U8)atoi(date[1]);
  //get the month
  ret.month = (U8)atoi(date[0]);
  //get the year
  ret.year = (U8)(atoi(date[2]) - 1970);

  //get the hour token. no need to tokenize, just chop at colon
  tokens[1][2] = '\0';
  ret.hr = atoi(tokens[1]);
  if(!strncmp(tokens[2],"PM",2)){ //add 12 hours for PM strings
    ret.hr += 12;
  }

  return ret;
}

/*
  Converts a text file to a gigantic string of words. Stopwords and invalid words are dropped and text is normalized.
*/
void kNN::textToWordSequence(const string& trainingFile, vector<string>& wordSequence, bool filterStopWords)
{
  int nTokens, i;
  U32 wordCt;
  char buf[BUFSIZE];
  char* toks[MAX_TOKENS_PER_READ];
  long double fsize, progress;
  string line, word, s;
  fstream infile(trainingFile.c_str(), ios::in);

  //stopfile.open(stopWordFile.c_str(), ios::read);
  if(!infile){
    cout << "ERROR could not open file: " << trainingFile << endl;
    return;
  }
  if(stopWords.size() == 0){
    cout << "ERROR stopwords table not yet built, cannot build co-occurrence table" << endl;
    return;
  }

  //gets the file size
  fsize = (long double)infile.tellg();
  infile.seekg(0, std::ios::end);
  fsize = (long double)infile.tellg() - fsize;
  infile.seekg(0, infile.beg);

  wordSequence.reserve(1 << 24); //reserve space for about 1.6 million words

  //cout << "max_size of list<string>: " << wordSequence.max_size() << "  fsize: " << fsize << endl; 

  wordCt = 0;
  while(infile.getline(buf,BUFSIZE)){  // same as: while (getline( myfile, line ).good())

    if( strnlen(buf,BUFSIZE) > 5){  //ignore lines of less than 10 chars

      //strncpy(buf,line.c_str(),255);
      buf[BUFSIZE-1] = '\0';
      s.clear();
      normalizeText(buf,s);
      strncpy(buf,s.c_str(),BUFSIZE-1);
      buf[BUFSIZE-1] = '\0';
      nTokens = tokenize(toks,buf,delimiters);

      //push each of these tokens to back of vector
      for(i = 0; i < nTokens; i++){
        word = toks[i];

        //verifies current gram is not a stop word
        if(isValidWordExtended(word) && filterStopWords && !isStopWord(word)){  //verifies current gram is not a stop word
          word = toks[i];
          wordSequence.push_back(word);
          wordCt++;
          //cout << word << " " << flush;
        }

        //no filtering except some basic validity checks
        if(isValidWord(word) && !filterStopWords){
          word = toks[i];
          wordSequence.push_back(word);
          wordCt++;
          //cout << word << " " << flush;
        }

        if((wordCt % 1000) == 0){
          progress = (long double)infile.tellg();
          cout << "\r" << (int)((progress / fsize) * 100) << "% complete wordSeq.size()=" << wordSequence.size() << "             " << flush;
        }
      }
    }
  }
  cout << endl;

  //NOT HERE! init numwords after we prune the vocabulary
  //this->numwords = wordSequence.size();

  /*
  i = 0;
  for(vector<string>::iterator it = wordSequence.begin(); it != wordSequence.end(); ++it){
    cout << " " << *it;
    i++;
    if(i % 20 == 0){
      cout << " " << endl;
    }
  }
  cout << "\nend of your list sir" << endl;
  */

  infile.close();
}

/*
  returns average list length, which is also the average word frequency of any word

  The average frequency of any word is a strong overestimate of the actual frequency for many words,
  since a few, very very common wrds throw off the average. But this is expected. The intent is to
  generate a pruning metric/heuristic to eliminate less common words, instead of having to run coDistance
  counts over night.

  Nonetheless, even in cases when you might want a less restrictive estimate, you could just use meanFreq/2,
  or something even more statistically robust (using variance, stdev, etc).
*/
U32 kNN::getMeanFrequency(IndexTable& wordTable)
{
  U64 sum, nlists, ret;
  IndexTableIt it;

  nlists = wordTable.size();
  sum = 0;
  for(it = wordTable.begin(); it != wordTable.end(); ++it){
    sum += it->second.second.size();
  }

  if(sum <= 0){
    cout << "ERROR sum<=0 in getMeanFrequency()" << endl;
    return 0;
  }

  //cout << "nlists=" << nlists << " sum=" << sum << endl;
  ret = (sum / nlists);
  if(sum % nlists){  // ceiling the divisor
    ret++;
  }

  //dbg: get num elements greater than mean
  for(sum = 0, it = wordTable.begin(); it != wordTable.end(); ++it){
    if(it->second.second.size() > ret){
      sum++;
    }
  }

  cout << "in prune, mu=" << ret << "  num-elements > mu: " << sum << endl;
  //cin >> sum;
  return (U32)ret;  //int division is intentional
}

/*
  Given some linear sequence of words, this builds a dictionary in which the keys are
  words, and the values are an integer list of all the occurrences (indices) for that word.
  This gives us a simple data structure for comparing two word-index sequences (eg, for
  average min distance relations).

  HACK: I hacked this function so it builds the occurrence dictionary, then iterates back over the
  dictionary, removing less-common terms. This cuts the complexity of correlating two sequences by reducing
  the size of the vocabulary. Its just pruning. But observe that correlating sequences requires |vocab|^2,
  which is simply not feasible for anything more than a few thousand word vocabulary, although you could run this
  for days over a very controlled data set to includes those as well.  I do determined pruning at 28 ONLY
  based on the enronParsed dataset, and this number will likely change per-dataset. The frequency distribution
  was very bottom-heavy, with about 80-85% of terms having a frequency less than 28 in this dataset. This is likely
  common with many language samples, and dropping the more unique aspects of a language still yields a very specific
  semantic model are correlating terms.

  Returns: the length of the longest list in the table. Returning this is just hackishly convenient for initializing
  a correlation vector to the longest possible index-sequence, so we don't have to constantly resize() it. Also
  means we don't have to re-iterate over the table looking for the longest sequence length, when needed.

  Doesn't initialize U16 word ids, since on start there may be > 65535 unique words. Only write the ids
  after pruning the word set.
*/
U32 kNN::buildWordIndexTable(const vector<string>& wordSequence)
{
  U32 max, i;
  vector<string>::iterator it;
  IndexTableIt maxit;
  IndexTableIt mapIt;

  //builds the sequence dictionary:  word -> occurrences[1,34,76,300...]
  for(i = 0; i < wordSequence.size(); i++){
    wordIndexTable[ wordSequence[i] ].second.push_back(i);
  }
  this->numwords = i;

  //dbg: find the max. this isn't necessary except for tracking purposes
  max = 0; i = 0;
  for(mapIt = wordIndexTable.begin(); mapIt != wordIndexTable.end(); ++mapIt){
    if(mapIt->second.second.size() > max){
      maxit = mapIt;
      max = mapIt->second.second.size();
    }
  }
  cout << "max word.ct " << max << " for: " << maxit->first << " in buildIndexTable  numwords=" << numwords << endl;
  return max;
}


/*
  Prints top words in index table, for manual inspection...
*/
void kNN::printTopWords(int n)
{
  U32 i;
  IndexTableIt mapIt;
  list<pair<string,U32> > wordList;
  list<pair<string,U32> >::iterator listIt;

  for(mapIt = wordIndexTable.begin(); mapIt != wordIndexTable.end(); ++mapIt){
    pair<string,U32> p;
    p.first = mapIt->first;
    p.second = mapIt->second.second.size();
    wordList.push_front(p);
  }
  wordList.sort(byFrequency_U32);
 
  cout << "most " << n << " frequent words in table, sorted by frequency: " << endl;
  for(i = 0, listIt = wordList.begin(); i < wordList.size() && i < n; i++, ++listIt){
    cout << i << ": " << listIt->first << "|" << listIt->second << endl;
  }
  wordList.clear();
}




/*
  Precondition: wordIndexTable built, pruned or not.
  Output will be sorted, since wordIndexTable is sorted.

kNN::wordIndexTableToFile(string& fname)
{
  IndexTableIt it;

  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){

  }

  int ofile, i, j;
  string ostr;
  IndexTableIt it;

  if(CoDistTable.empty()){
    cout << "ERROR distTable empty in coDistTableToFile()" << endl;
    return;
  }
  ofile = open(fname.c_str(),O_WRONLY | O_CREAT);
  if(ofile < 0){
    cout << "ERROR coDistTableToFile() could not open file >" << fname << "<" << endl;
    return;
  }

  i = 0;
  ostr.reserve(1 << 12);
  //build a very large list of all the records in the coDistTable, as strings. then sort and output to file.
  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    ostr += it->first;
    for(j = 0; j < it->second.size(); j++){
      ostr += "\t";
      ostr += std::to_string(inner->second[j]);
      //olist.push_front(ostr);
    }
    ostr += "\n";
    i++;
    if((i % 150) == 149){
      write(ofile,ostr.c_str(),ostr.size());
      ostr.clear();
    }
  }

  //the next method was for sorting the output, which blew up memory (sorting ~12 million strings)
  //instead, just build the models with regular <map> type, which is inherently ordered. they are slower,
  //but effectively pre-sort the output by key1. So compile and build/output with <map>, then just save the file!
  cout << "num uniq keys is: " << i << endl; 

  close(ofile);




}
*/

/*
  Prunes the table to cut down on sparsity. This way we only calculate co-distances for common terms.
*/
void kNN::pruneWordIndexTable(IndexTable& wordTable)
{
  int temp;
  U32 pruneThreshold, max, i;
  IndexTableIt mapIt, target;
  //IndexTable tempTable;

  pruneThreshold = 0;
  temp = getMeanFrequency(wordTable) - 15; //HACK add ten to get a heavily pruned set
  if(temp > 0){
    pruneThreshold = temp;
  }

//pruneThreshold = argmax(pruneThresh:old,85); //cap the number of distance comparisons at 4k
  //pruneThreshold = getMeanFrequency(wordIndexTable) + 10; //HACK add ten to get a heavily pruned set
  cout << "(currently using lazy avg)  pruneThreshold=" << pruneThreshold << "  init table size: " << wordIndexTable.size() << endl;
  //U32 lt14 = 0; U32 lt28 = 0; U32 gt28 = 0; U32 gt42 = 0; U32 lt7 = 0;
  //drops less-common terms from the dict
  for(max = 0, mapIt = wordTable.begin(); mapIt != wordTable.end(); ){
    //drop the least frequent terms to resolve some of the data sparsity
    if(mapIt->second.second.size() <= pruneThreshold){
      target = mapIt;
      ++mapIt;
      this->numwords -= target->second.second.size();  //update numwords after removing these words
      wordTable.erase(target);
    }
    else{
      if(mapIt->second.second.size() > max){ //store max length
        max = mapIt->second.second.size();
      }
      ++mapIt;
    }
  }

  cout << "post-prune, wordIndexTable.size() is: " << wordIndexTable.size() << " idCounter=" << idCounter << endl;
}

//converts key1,key2,dist into an output string for writing to a file. appends to output string so we can buffer up a bunch of these
//line in the output string.
void kNN::appendToOutputString(string& ostr, const string& k1, const string& k2, const string& distStr)
{
  ostr += k1; ostr += "\t"; ostr += k2; ostr += "\t"; ostr += distStr; ostr += "\n";
}


//a terribly nested, single context function  
bool kNN::ShareCoOccurrences(CoDistanceTable& simTable, RowMap& row, U32 key1, U32 key2)
{
  CoDistOuterIt outer;
  CoDistInnerIt it;

  if(simTable.empty() || row.empty()){ //logic exception
    return false;
  }

  //look in current row first (this is like checking the original table for [key1][key2], since "row" already covers the [key1]
  it = row.find(key2);
  if(it != row.end()){
    return true;
  }

  //look ahead
  outer = simTable.find(key2);
  if(outer != simTable.end()){
    it = outer->second.find(key1);
    if(it != outer->second.end()){
      return true;
    }
  }

  return false;
}

/*check if the CoDist table already has some key, which
  is defined as (word1,word2) in either order. Since word1+word2
  and word2+word1 are technically unique keys, this function
  allows us to conflate them as ne logical key. This halves the potential
  size of our table by eliminating duplication, as the following is true:
    dist(word1,word2) == dist(word2,word1)
  Therefore, CoDist[word1][word2] should always equal CoDist[word2][word1], and we
  can eliminate at least one of these entries.
  
  Returns false if neither dict[k1][k2] nor dict[k2][k1] exists in table
  If true, iterator of location is stored in _it.

*/
bool kNN::CoDistTableHasKey(CoDistanceTable& coDistTable, const U32 key1, const U32 key2, CoDistInnerIt& it)
{
  CoDistOuterIt outer;

  if(coDistTable.empty()){ //logic exception
    return false;
  }

  outer = coDistTable.find(key1);
  if(outer != coDistTable.end()){
    it = outer->second.find(key2);
    if(it != outer->second.end()){
      return true;
    }
  }
  outer = coDistTable.find(key2);
  if(outer != coDistTable.end()){
    it = outer->second.find(key1);
    if(it != outer->second.end()){
      return true;
    }
  }

  return false;
}

bool kNN::CoDistTableHasKey(CoDistanceTable& coDistTable, const string& k1, const string& k2, CoDistInnerIt& it)
{
  U32 key1 = strToId(k1);
  U32 key2 = strToId(k2);

  if(key1 == 0){
    cout << "ERROR key1=0 for k1=" << k1 << " in CoDistTableHasKey" << endl;  
    return false;
  }
  if(key2 == 0){
    cout << "ERROR key2=0 for k2=" << k2 << " in CoDistTableHasKey" << endl;  
    return false;
  }
  if(coDistTable.empty()){ //logic exception
    return false;
  }

  return CoDistTableHasKey(coDistTable,key1,key2,it);

/*
  if(CoDistTable.find(key1) != CoDistTable.end()){
    if(CoDistTable[key1].find(key2) != CoDistTable[key1].end()){
      return true;
    }
  }
  if(CoDistTable.find(key2) != CoDistTable.end()){
    if(CoDistTable[key2].find(key1) != CoDistTable[key2].end()){
      return true;
    }
  }
  return false;
*/
}

/* iterator version of the above. Like map.find()
  //look for first key permutation
  ret = CoDistTable[key1].find(key2);
  if(ret != CoDistTable[key1].end()){
    return ret;
  }

  //look for second key permutation
  ret = CoDistTable[key2].find(key1);
  if(ret != CoDistTable[key2].end()){
    return ret;
  }
  return ret;
}
*/

//print the top collocates to view semantic relationships by rank
//dont call this. the output list will be several million strings, sys mem will bonk out.
void kNN::printTopCollocates(void)
{
  int i;
  char dummy;
  string s, s1, s2;
  CoDistOuterIt outer;
  CoDistInnerIt inner;
  list<pair<string,U32> > colList;
  list<pair<string,U32> >::iterator it;

  if(CoDistTable.empty()){
    cout << "CoDistTable empty" << endl;
    return;
  }

  cout << "Top collocates:" << endl;

  //just build a raw list of all elements, then call sort
  for(outer = CoDistTable.begin(); outer != CoDistTable.end(); ++outer){
    for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      s1 = idToStr(outer->first);
      s2 = idToStr(inner->first);
      s = s1;
      s += " ";
      s += s2;
      pair<string,U32> p(s,(U32)inner->second);
      colList.push_front(p);
    }
  }
  colList.sort(compareCoDistTableIt);

  for(i = 0, it = colList.begin(); it != colList.end() && i < 150; i++, ++it){
    cout << i << " " << it->first << " " << it->second << endl;
    if((i % 50) == 49){
      cin >> dummy;
    }
  }
  cout << "complete" << endl;
}

/*
  Word idcounter gets updated by incrementing, which means we are creating new table keys
  with a linearly-increasing value. For some map data structures this risks simple making a linked list.
*/
U32 kNN::allocId(const string& word, IndexTableIt it)
{
  if(idCounter >= U32MAX){
    cout << "ERROR out of id's in allocId" << endl;
    return 0;
  }

  //update both tables, so they are in sync
  ++idCounter;
  wordIdTable[idCounter] = word;
  it->second.first = idCounter;

  return idCounter;
}

U32 kNN::allocId(const string& word)
{
  if(idCounter >= U32MAX){
    cout << "ERROR out of id's in allocId" << endl;
    return 0;
  }

  //update both tables, so they are always in sync
  ++idCounter;
  wordIdTable[idCounter] = word;
  wordIndexTable[word].first = idCounter;

  return idCounter;
}

/*
  Finds word in id table; if not found, attempts to alloc a new id.
*/
U32 kNN::strToId(const string& word)
{
  U32 id;
  IndexTableIt it = wordIndexTable.find(word);

  if(it == wordIndexTable.end()){
    //cout << "WARN word-id not found for >" << word << "< attempting to alloc new id. Verify wordIndexTable and wordIdTable alignment." << endl;
    id = allocId(word);
    if(id == 0){
      cout << "ERROR out of keys in getId()" << endl;
    }
  }
  else{
    id = it->second.first;
  }

  return id;
}

//convert a u32 id back to its string
string kNN::idToStr(const U32 id)
{
  string word;
  IdTableIt it = wordIdTable.find(id);

  if(it == wordIdTable.end()){
    cout << "ERROR word-id not found for >" << word << "<" << endl;
    word = "NIL";
  }
  else{
    word = it->second;
  }

  return word;
}




/*
  increment a co-location count for two words, key1 and key2

  This function is meant to implicitly filter dupe key permutations from the table.
*/
void kNN::updateCoDistTable(const string& key1, const string& key2)
{
  U16 k1, k2;
  CoDistOuterIt outer;
  CoDistInnerIt inner;

  k1 = strToId(key1);
  k2 = strToId(key2);
  if(k1 <= 0){
    cout << "ERROR k1=0 for key1=" << key1 << " in updateCoDistTable" << endl;  
    return;
  }
  if(k2 <= 0){
    cout << "ERROR k2=0 for key2=" << key2 << " in updateCoDistTable" << endl;
    return;
  }

/*
  if(CoDistTable.empty()){ //logic exception
    CoDistTable[k1][k2] = 1.0;
    return;
  }
*/

  outer = CoDistTable.find(k1);
  //check first ordering: key1+key2
  if(outer != CoDistTable.end()){
    inner = outer->second.find(k2);
    if(inner != outer->second.end()){
      inner->second++;
      return;
    }
  }

  outer = CoDistTable.find(k2);
  //check second ordering: key2+key1
  if(outer != CoDistTable.end()){
    inner = outer->second.find(k1);
    if(inner != outer->second.end()){
      inner->second++;
      return;
    }
  }

  //only reach here if neither key1+key and key2+key1 found in table
  CoDistTable[k1][k2] = 1.0;
}

/*
void kNN::lemmatizeWordSequence(vector<string>& wordSequence)
{
  if(lemmaTable.empty()){
    cout << "ERROR lemmaTable empty in lemmatizeWordSequence(), could not lemmatize" << endl;
    return;
  }

  for(vector<string>::iterator it = wordSequence.begin(); it != wordSequence.end(); ++it){
    map<string, string>::iterator mit = lemmaTable.find(*it);
    if(mit != lemmaTable.end()){
      cout << *it << "lemmatized to: " << mit->second << endl;
      *it = mit->second;
    }
  }
}
*/

/*
  Aligns a word sequence with the vocabulary in the wordIndexTable
  This is a very nested (inner) function. In other word it has a very stateful context:
  only called after wordIndexTable has been built, and likely pruned.
*/
void kNN::pruneWordSequence(const vector<string>& inVec, vector<string>& outVec)
{
  U32 i;

  cout << "beginning pruneWordSequence(), inSeq.size()=" << inVec.size() << endl;
  //if a word is in the wordIndexTable vocab, push it to stack. At end, clear <words> and push all of stack back
  //onto words. At a high memory costs, this avoids the huge overhead of deleting from vectors, if still inefficient.
  for(i = 0; i < inVec.size(); i++){
    if(wordIndexTable.find(inVec[i]) != wordIndexTable.end()){
      outVec.push_back(inVec[i]);
    }
  }

/*
  words.clear();

  for(i = 0; i < stack.size(); i++){
    words.push_back(stack[i]);
  }
*/
  cout << "end of pruneWordSequence(), outSeq.size()=" << outVec.size() << endl;
}

void kNN::copyVecs(const vector<string>& src, vector<string>& dest)
{
  dest.clear();
  for(U32 i = 0; i < src.size(); i++){
    dest.push_back(src[i]);
  }
}

/*
  Note the pre/post conditions of this function carefully, especially wrt the wordIndexTable and the wordSequence

  Init state: let wordSequence and wordIndexTable be raw, unpruned, though based off one another.
  Post-state: CoDistTable is built, wordIndexTable pruned, and the vocabulary of the two is 1:1 (aligned)

  wordSequence is DESTROYED! to reduce high-memory consumption. Just be aware.
*/
//void kNN::buildCoDistTable(CoDistanceTable& coDistTable, vector<string>& wordSequence)
void kNN::buildCoDistTable(vector<string>& wordSequence)
{
  //U64 sum;
  U32 i, ct, radius, maxlen, left, pivot, right;
  IndexTableIt outer;  //iterates over each word in the map
  CoDistOuterIt cd_outer;
  CoDistInnerIt cd_inner, dupe;
  //CoDistanceTable coOccurrenceTable;

  if(wordSequence.empty()){
    cout << "ERROR wordSequence empty in buildCoDistTable, build failed" << endl;
    return;
  }

/*
  ofile = open(dbfile.c_str(),O_WRONLY | O_CREAT);
  if(ofile < 0){
    cout << "ERROR could not open file: " << dbfile << endl;
    return;
  }
*/

  /****** builds the essential data structures ******/
  //TODO: lemmatization
  //lemmatizeWordSequence(wordSequence);
  //now build the word index table from the wordSequence, appending indices of word's occurence to its key in map
  maxlen = 0;
  if(wordIndexTable.empty()){
    cout << "WARN Attempting to build wordIndexTable in buildCoDistTable" << endl;
    vector<string> filteredSequence; //contains only words whose frequency exceeds some threshold (often the mean frequency of the set)
    maxlen = buildWordIndexTable(wordSequence);
    //cout << "before prune, wordIndexTable.size()=" << wordIndexTable.size() << endl;
    pruneWordIndexTable(wordIndexTable); //pruning wordIndexTable is a pre-condition for calling pruneWordSequence()
    pruneWordSequence(wordSequence, filteredSequence);  //align word sequence with pruned wordIndexTable, to prevent a lot of out-of-vocabulary noise
    clearWordIndexTable();
    cout << "wordIndexTable cleared, filteredSequence.size()=" << filteredSequence.size() << endl;
    buildWordIndexTable(filteredSequence); //rebuild the table based on filtered sequence, so index lists are valid for this sequence
    initWordIdTable();
    cout << "after seq prune, wordIndexTable.size()=" << wordIndexTable.size() << endl;
    copyVecs(filteredSequence,wordSequence);
  }

  if(wordIndexTable.size() != wordIdTable.size()){
    cout << "WARN wordIndexTable.size()=" << wordIndexTable.size() << " but wordIdTable.size()=" << wordIdTable.size() << " in buildCoDistTable()" << endl;
  }


  //cout << "after prune, wordIndexTable.size()=" << wordIndexTable.size() << endl;
  //dont need the wordSequence anymore, so clear it out since it eats up a lot of memory:
  ///wordSequence.clear();
  /**************************************************/

  //TODO: arguments can be made whether or not numwords should be post or pre-prune, on the validity of the resulting probability dist.
  //this->numwords = wordIndexTable.size();

  //TODO: THIS MAY STILL BE ITERATING OVER OUT-OF-VOCAB WORDS, POST PRUNE() (WORD SEQUENCE IS NOT PRUNED, INDEX TABLE IS)

  //remember the distribution of occurrence list lengths is very important. usually these are very bottom heavy, and the average is thrown by large outliers
  //cout << "wordIndexTable.size()=" << wordIndexTable.size() << " maxLen=" << maxlen << endl;
  cout << "building codistance table from wordIndexTable keys related over filtered word sequence.\n"
       << "NOTE if wordIndexTable has been pruned, CoDistTable keys will represent only a relatively\n"
       << "small (~10-15%) subset of the raw word sequence. Also note how distance vals imply a\n"
       << "probabilistic scalar between terms..." << endl;

  //Correlate each word in the sequence with every other word in the sequence
  radius = (U32)CODIST_DIAMETER >> 1;
  cout << "building with radius=" << radius << endl;
  ct = 0;
  //for each word index, look *radius* words back and forward to find related words
  for(outer = wordIndexTable.begin(); outer != wordIndexTable.end(); ++outer){
    //iterate over the word indices: [34,56,234,345] for this word
    for(i = 0; i < outer->second.second.size(); i++){
    
      //get the word index bounds for the evaluation window of *radius* words to left and right
      pivot = outer->second.second[i];
      if(radius >= pivot)  //same as argmax(pivot-radius,0). This is easier for unsigned.
        left = 0;
      else
        left = pivot - radius;
      right = argmin(pivot+radius,(U32)wordSequence.size());
    
      if(left >= right){
        cout << "WARN left=" << left << " >= right=" << right << " radius=" << radius << " pivot=" << pivot << " wordSeq.size()=" << wordSequence.size() << " in buildCoDistTable()" << endl;
      }

      //increment each co-occurrence
      while(left < right){
        if(wordSequence[left] != wordSequence[pivot]){ //check verifies we're not correlating a word with itself
          updateCoDistTable(outer->first,wordSequence[left]); //this function ensures key symmetry, so no dupes in table
        }
        left++;
      }
    }
  
    ct++;
    if(ct % 200 == 0){
      cout << "\rct: " << ct << " of " << wordIndexTable.size() << " items. pivot=" << outer->first << "         " << flush;
    }
  }
  cout << endl;
  //after loop: codist table contains co-locate counts

  wordSequence.clear(); //clear this list, since its a memory hog

/*  //dbg
  for(i = 0, cd_outer = CoDistTable.begin(); (i < 300) && cd_outer != CoDistTable.end(); ++cd_outer){
    for(cd_inner = cd_outer->second.begin(); cd_inner != cd_outer->second.end(); ++cd_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      i++;
      cout << cd_outer->first << " " << cd_inner->first << "|" << cd_inner->second << endl;
    }
  }
*/



/*
  cout << "trimming dupes from table, init size=" << CoDistTable.size() << endl;
  //previous algorthm using update() function should have prevented dupes, so this is not needed
  for(i = 0, cd_outer = CoDistTable.begin(); cd_outer != CoDistTable.end(); ++cd_outer){
    for(cd_inner = cd_outer->second.begin(); cd_inner != cd_outer->second.end(); ++cd_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      dupe = CoDistTable[cd_inner->first].find(cd_outer->first);
      if(dupe != CoDistTable[cd_inner->first].end()){
        CoDistTable[cd_inner->first].erase(dupe);
        i++;
      }
    }
  }
  cout << "removed >" << i << "< dupes. table size is now=" << CoDistTable.size() << endl;
*/

  struct timespec begin, end;
  clock_gettime(CLOCK_MONOTONIC,&begin);
  buildSimilarityTable(CoDistTable);
  clock_gettime(CLOCK_MONOTONIC,&end);
  end = DiffTimeSpecs(&begin,&end);
  cout << "\nsim table build() time: " << end.tv_sec << "s " << end.tv_nsec << "ns" << endl;

/*
  //final step, correlate the words in the table using ??? Bordag equation (5)
  //however this is done, the important thing is just that we precompute these values
  for(cd_outer = CoDistTable.begin(); cd_outer != CoDistTable.end(); ++cd_outer){
    for(cd_inner = cd_outer->second.begin(); cd_inner != cd_outer->second.end(); ++cd_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      cd_inner->second = buildSimilarityTable(cd_outer->first, cd_inner->first);
    }
    cout << "\rcalc, outer i=" << (i++) << " of " << CoDistTable.size() << "                       " << flush;
  }
*/


/*
  i = 0;
  //final step, correlate the words in the table using ??? Bordag equation (5)
  //however this is done, the important thing is just that we precompute these values
  for(cd_outer = CoDistTable.begin(); cd_outer != CoDistTable.end(); ++cd_outer){
    for(cd_inner = cd_outer->second.begin(); cd_inner != cd_outer->second.end(); ++cd_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      cd_inner->second = calculateSignificance(cd_outer->first, cd_inner->first);
    }
    cout << "\rcalc, outer i=" << (i++) << " of " << CoDistTable.size() << "                       " << flush;
  }
  cout << endl;
*/

  coDistTableToFile(dbfile);

  cout << "DONE >> CoDistTable.size(): " << CoDistTable.size() << endl;
}



/*
  Calculates the cosine between two word co-occurrence vectors
*/
void kNN::buildSimilarityTable(CoDistanceTable& similarityTable)
{
  double progress, outerprogress;
  FloatType angle;
  CoDistOuterIt cd_outer1, cd_outer2;
  CoDistInnerIt inner;
  //vector<FloatType> row;
  RowMap row;

/*
  CoDistOuterIt cd_outer1 = coOccurrenceTable.find(k1);
  CoDistOuterIt cd_outer2 = coOccurrenceTable.find(k2);

  return cd_outer1->second[cd_outer2->first] = method_binaryCosine(cd_outer1,cd_outer2);
*/
  
  //row.reserve(similarityTable.begin()->second.size());

  outerprogress = 0.0;
  //for each word, relate it with every other word (including itself, currentlys)
  for(cd_outer1 = similarityTable.begin(); cd_outer1 != similarityTable.end(); ++cd_outer1){
    progress = 0.0;
    //cout << "outer1=" << cd_outer1->first;

    //copy the row into a read-only temp map, since the row is being written-to and used for calculations simultaneously in the next loop
    if(!row.empty()){
      row.clear();
    }
    for(inner = cd_outer1->second.begin(); inner != cd_outer1->second.end(); ++inner){
      row[inner->first] = inner->second;
      //row.push_back(inner->second);
    }
    //delete the write-to map
    cd_outer1->second.clear();
    //cout << "sizes: " << row.size() << " " << cd_outer1->second.size() << endl;

    cd_outer2 = cd_outer1;
    for(cd_outer2++; cd_outer2 != similarityTable.end(); ++cd_outer2){
      //cout << "==" << cd_outer1->first << " ??" << endl;
      //HACK a space heuristic, to only compare words that share at least one co-occurrence
      if(ShareCoOccurrences(similarityTable,row,cd_outer1->first,cd_outer2->first)){ //a really, really ugly function...
      //if(CoDistTableHasKey(similarityTable,cd_outer1->first,cd_outer2->first,inner)){
        //inner->second = method_binaryCosine(cd_outer1,cd_outer2);
        angle = method_weightedCosine(row,cd_outer2->second);
        if(isnormal(angle)){  //prevents nans
          similarityTable[cd_outer1->first][cd_outer2->first] = angle;
        }
        else{
          if(angle != 0.0){
            cout << "WARN abnormal value >" << angle << "< for " << cd_outer1->first << "\\" << cd_outer2->first << endl;
          }
          //don't waste space on zero entries, just return 0.0 for non-hits in the look up methods
          //similarityTable[cd_outer1->first][cd_outer2->first] = 0.0;
        }
        //cout << "similarity " << wordIdTable[cd_outer1->first] << "|" << wordIdTable[cd_outer2->first]<< ": " << inner->second << endl;
        /*
        if(inner->second > 0.95){
          cout << "similarity " << wordIdTable[cd_outer1->first] << "|" << wordIdTable[cd_outer2->first]<< ": " << inner->second << endl;
        }
        */
      }

      //progress++;
      //cout << "inner progress: " << (100 * (progress / (double)cd_outer1->second.size())) << "% completed             \r" << flush;
    }
    outerprogress++;
    cout << "\router progress: " << (100 * (outerprogress / (double)CoDistTable.size())) << "% completed       " << flush;
  }
}

//compares two words as co-occurrence vectors
FloatType kNN::method_binaryCosine(RowMap& row1, RowMap& row2)
{
  bool hit1, hit2;  //flags for if some word has a co-occurrence (any non-zero value) for some other word
  FloatType n12, n1, n2;
  CoDistInnerIt inner;

  n1 = row1.size();
  n2 = row2.size();

  n12 = 0.0;
  //iterate in parallel over both co-occurrence vectors
  for(U32 i = 0; i < idCounter; i++){

    //check if both word1 and word2 map to some other word.
    if(row1.find(i) != row1.end() && row2.find(i) != row2.end()){
      n12++;
    }
/*
    hit1 = hit2 = false;
    inner = row1.find(i);
    //boolean indicates word has co-occurrence with id=i.
    if(inner != row1.end() && inner->second > 0.0){
      n1++;
      hit1 = true;
    }
    inner = row2.find(i);
    //boolean indicates word has co-occurrence with id=i.
    if(inner != row2.end() && inner->second > 0.0){
      n2++;
      hit2 = true;
    }
    if(hit1 && hit2){
      n12++;
    }
*/
  }
  //post-loop: have counts of n-bits in vec1, vec2, and vec1 X vec2 (intersection/shared bits)

  //zero check to prevent nans
  if(n1 > 0.0 && n2 > 0.0){
    //assign binary cosine val to w1 x w2
    return (FloatType)(n12 / (sqrt(n1) * sqrt(n2)));
  }
  
  //return 0.0 for words which shared no second-order co-occurrences
  return 0.0;
}


//compares two words as co-occurrence vectors. Faster version. 
FloatType kNN::method_weightedCosine(RowMap& row1, RowMap& row2)
{
  FloatType n12, ret, n1, n2;
  CoDistInnerIt inner1, inner2;

  ret = n12 = n1 = n2 = 0.0;
  
  //iterate SMALLER row, correlating with larger set. This speeds things up by looking up only the largest possible size of the set intersection (the size of the smaller set)
  //This will normally be very effective, since most set sizes will be somewhat different.
  if(row1.size() < row2.size()){
    //row1 is smaller, so iterate that set
    for(inner1 = row1.begin(); inner1 != row1.end(); ++inner1){
      n1 += pow(inner1->second,2.0);

      inner2 = row2.find(inner1->second);
      if(inner2 != row2.end()){
        n12 += (inner1->second * inner2->second);
      }
    }
    //post-loop: n1 and n12 counts set

    //...and now get n2 count
    for(inner2 = row2.begin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);
    }
  }
  else{
    //row2 is the smaller set, so use it to perform lookups in the other set
    for(inner2 = row2.begin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);

      inner1 = row1.find(inner2->second);
      if(inner1 != row1.end()){
        n12 += (inner2->second * inner1->second);
      }
    }
    //post-loop: n2 and n12 counts set
    
    //...and now get n1 count
    for(inner1 = row1.begin(); inner1 != row1.end(); ++inner1){
      n1 += pow(inner1->second,2.0);
    }
  }
  //post: have counts of n1, n2, and n12

  //zero check to prevent nans
  if(n12 > 0.0 && n1 > 0.0 && n2 > 0.0){
    //assign binary cosine val to w1 x w2
    ret =  (FloatType)(n12 / (sqrt(n1) * sqrt(n2)));
  }
  
  //returns 0.0 for words which shared no second-order co-occurrences
  return ret;
}

/*
//compares two words as co-occurrence vectors. 
FloatType kNN::method_weightedCosine(RowMap& row1, RowMap& row2)
{
  FloatType n12, ret, n1, n2;
  CoDistInnerIt inner1, inner2;

  ret = n12 = n1 = n2 = 0.0;
  //iterate in parallel over both co-occurrence vectors
  for(U32 i = 0; i < idCounter; i++){

    inner1 = row1.find(i);
    if(inner1 != row1.end()){
      n1 += pow(inner1->second,2.0);
    }
    inner2 = row2.find(i);
    if(inner2 != row2.end()){
      n2 += pow(inner2->second,2.0);
    }
    if(inner1 != row1.end() && inner2 != row2.end()){
      n12 += (inner1->second * inner2->second);
    }
  }
  //post-loop: have counts of n-bits in vec1, vec2, and vec1 X vec2 (intersection/shared bits)

  //zero check to prevent nans
  if(n1 > 0.0 && n2 > 0.0 && n12 > 0.0){
    //assign binary cosine val to w1 x w2
    ret =  (FloatType)(n12 / (sqrt(n1) * sqrt(n2)));
  }
  
  //returns 0.0 for words which shared no second-order co-occurrences
  return ret;
}
*/

/*
  Precondition: wordIdTable is still in memory, so we can loop up strings
  by id, then output the strings.
*/
void kNN::coDistTableToFile(string& fname)
{
  int ofile, i;
  string ostr;
  CoDistOuterIt outer;
  CoDistInnerIt inner;
  list<string> olist;

  if(CoDistTable.empty()){
    cout << "ERROR distTable empty in coDistTableToFile()" << endl;
    return;
  }
  ofile = open(fname.c_str(),O_WRONLY | O_CREAT);
  if(ofile < 0){
    cout << "ERROR coDistTableToFile() could not open file >" << fname << "<" << endl;
    return;
  }

  cout << "writing codist table to file: " << fname << endl;

  i = 0;
  ostr.reserve(1 << 12);
  //build a very large list of all the records in the coDistTable, as strings. then sort and output to file.
  for(outer = CoDistTable.begin(); outer != CoDistTable.end(); ++outer){
    for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      ostr += idToStr(outer->first);
      ostr += "\t";
      ostr += idToStr(inner->first);
      ostr += "\t";
      ostr += std::to_string(inner->second);
      ostr += "\n";
      //olist.push_front(ostr);

      i++;
      if((i % 150) == 149){
        write(ofile,ostr.c_str(),ostr.size());
        ostr.clear();
      }
    }
  }

  //flush remaining string contents
  if(ostr.length() > 0){
    write(ofile,ostr.c_str(),ostr.size());
  }

  //the next method was for sorting the output, which blew up memory (sorting ~12 million strings)
  //instead, just build the models with regular <map> type, which is inherently ordered. they are slower,
  //but effectively pre-sort the output by key1. So compile and build/output with <map>, then just save the file!
/*
  cout << "sort... olist.size()=" << olist.size() << endl;
  olist.sort();
  for(list<string>::iterator it = olist.begin(); it != olist.end(); ++it){
    //cout << *it << endl;
    write(ofile,it->c_str(),it->size());
  }
  cout << "CoDistTable written to file: " << fname << endl;
*/
  cout << "write complete. num uniq keys in codist table: " << i << endl; 

  close(ofile);
}



/*
  Build the co-occurence table.
  ../../nGram/enronParsed/parsed.txt
*/
//void kNN::buildCoDistTable(const string& trainingFile)
//OLD version: see v3.0


/*
  Returns the min average distance measure between two integer sequences.
  The process is equivalent to building an m x n matrix of values, then
  going across each row taking the min value to represent that row to represent
  the min distance between two closest neighbor points in each sequence.
  Its ugly, and really only makes sense on a white board.

  Pre-condition: outer should be the longer of the two sequences. Again, this is for
  a smoother closest neigbor analysis, and must be shown on a whiteboard.

  This was a very nice, but complex function. It still works, just needs to be updated to meet the new
  data model interface of U16 word ids.


U32 kNN::getAvgMinDist(IndexTableIt& outer, IndexTableIt& inner, vector<U32>& minDist)
{
  U32 row, col, sum, dist;  //row and col are just logical descriptors; not using actual matrix[][] data structure, although the logic is equivalent

  //n^2 exhaustive traversal of both sequences to get mean of mins. awful, but okay for now
  for(row = 0; row < outer->second.size(); row++){
    for(col = 0; col < inner->second.size(); col++){

      if(outer->second[row] > inner->second[col]){ //this check just essentially averts an abs() call
        dist = outer->second[row] - inner->second[col];
      }
      else{
        dist = inner->second[col] - outer->second[row];
      }

      //dbg
      if(dist == 0){
        cout << "ERROR dist==0 in buildCoTable(): " << outer->first << " " << inner->first << endl;
      }

      if(dist < minDist[row]){ //yes, row is the correct index, not col
        minDist[row] = dist;
      }
    }
  }

  //average the min-distances, extending to the range of the longer sequence
  for(sum = 0, row = 0; row < outer->second.size() && minDist[row] != 0; row++){
    sum += minDist[row];
  }
  row--;

  //this line should never succeed, since minDist should have been initialized to the maximum possible list size in the table
  if(minDist[row] == 0){
    cout << "ERROR mindDist end-of-buffer reached in getAvgMinDist. See code for this exception, this line should never throw if bufs properly upper-bounded" << endl;
  }

  return (sum / row); //integer division is intentional
}
*/




/*  //extremely difficult implementation
      //iterate over the inner sequence, comparing with neighbors in outer sequence
      for(i = 0; i < inner->second.length(); i++){       
        //set left to: floor( (prev+cur)/2 )
        if(i != 0){
          left = (inner->second[i] + inner->second[i-1]) >> 2;  //floor
        }
        else{ //else take max of inner[0] and outer[0]
          left = max(inner->second[0], outer->second[0]);
        }
        //set right to: ceil( (next+cur)/2  )
        if(i != inner->second.length()){
          right = ((inner->second[i+1] + inner->second[i]) >> 2) + ((inner->second[i+1] + inner->second[i]) % 2);  //mod to get ceiling instead of floor
        }
        else{ //else take max of inner[end] and outer[end]
          right = max(inner->second[inner->second.length()-1], outer->second[outer->second.length()-1]);
        }
      //find left
      for()
      //find right
*/

/*
  overload to support char*
*/
bool kNN::isValidWordExtended(const char* word)
{
  string w = word;
  return isValidWordExtended(w);
}

/*
  Return true if token contains only numbers or slashes (such as date strings),
  no dollar signs, ampersands, colons, @, etc
  
  Excludes any words containing: 0123456789@:;<=>?#$%&[]^`_
*/
bool kNN::isValidWordExtended(const string& token)
{
  //no words longer than 64 char
  if(token.length() > MAX_WORD_LEN){
    //cout << "\rWARN unusual length word in isValidWord: >" << token << "< ignored. Check parsing                        " << endl;
    return false;
  }

  if(token[0] == '\''){  // filters much slang: 'ole, 'll, 'em, etc
    return false;
  }
  if((token[0] == '*') || (token[1] == '*')){
    return false;
  }
  if((token.length() == 3) && token[0] == 'c' && token[1] == 'o' && token[2] == 'm'){  //lots of "www"
    //cout << "invalid word: " << token << endl;
    return false;
  }
  if((token.length() == 3) && token[0] == 'w' && token[1] == 'w' && token[2] == 'w'){  //lots of "www"
    //cout << "invalid word: " << token << endl;
    return false;
  }
  if((token.length() >= 4) && token[0] == 'h' && token[1] == 't' && token[2] == 't' && token[3] == 'p'){  //lots of "http"
    //cout << "invalid word: " << token << endl;
    return false;
  }

  if((token.length() == 1) && (token[0] != 'a') && (token[0] != 'i')){  //hack: need to figure out why single letters are getting into the output, without this check (garbage in the enronSent?)
    return false;
  }
  if((token[0] == '\'') && ((token[1] == '\'') || (token[1] == 's'))){  //hack: covers '' and 's in output stream
    return false;
  }
  if(token == "th"){  //occurs when "8th" is converted to "th" after numeric drop
    return false;
  }

  for(U32 i = 0; i < token.length(); i++){
    if((token[i] >= 47) && (token[i] <= 64)){ ///exclude all of 0123456789@:;<=>?
      //cout << "invalid word: " << token << endl;
      return false;
    }
    if((token[i] >= 35) && (token[i] <= 38)){ ///exclude all of #$%&
      //cout << "invalid word: " << token << endl;
      return false;
    }
    if((token[i] >= 91) && (token[i] <= 96)){ ///exclude all of []^`_
      //cout << "invalid word: " << token << endl;
      return false;
    }
  }

  return true;
}

//revious isValid checks did heavy word-filtering. These ones are more minimal, for when we're trying to get normal language output.
bool kNN::isValidWord(const char* word)
{
  string w = word;
  return isValidWord(w);
}
bool kNN::isValidWord(const string& token)
{
  //no words longer than limit
  if(token.length() > MAX_WORD_LEN){
    //cout << "\rWARN unusual length word in isValidWord: >" << token << "< ignored. Check parsing                        " << endl;
    return false;
  }

  if(token[0] == '\''){  // filters much slang: 'ole, 'll, 'em, etc
    return false;
  }
  if((token[0] == '*') || (token[1] == '*')){
    return false;
  }
  if(token == "com"){  //lots of "www" and ".com" etc
    //cout << "invalid word: " << token << endl;
    return false;
  }
  if(token == "www"){  //lots of "www"
    //cout << "invalid word: " << token << endl;
    return false;
  }
  if(token == "http"){  //lots of "http"
    //cout << "invalid word: " << token << endl;
    return false;
  }

  if((token[0] == '\'') && ((token[1] == '\'') || (token[1] == 's'))){  //hack: covers '' and 's in output stream
    return false;
  }
  if(token == "th"){  //occurs when "8th" is converted to "th" after numeric drop
    return false;
  }

  for(U32 i = 0; i < token.length(); i++){
    if((token[i] >= 47) && (token[i] <= 64)){ ///exclude all of 0123456789@:;<=>?
      //cout << "invalid word: " << token << endl;
      return false;
    }
    if((token[i] >= 35) && (token[i] <= 38)){ ///exclude all of #$%&
      //cout << "invalid word: " << token << endl;
      return false;
    }
    if((token[i] >= 91) && (token[i] <= 96)){ ///exclude all of []^`_
      //cout << "invalid word: " << token << endl;
      return false;
    }
  }

  return true;
}


//completely randomized version. other version favors more likely words, which isn't good. this tests a randomized mean version
void kNN::initRandomMeans(const int k, vector<string>& means)
{
  int i, j, r, l;
  IndexTableIt it;
  list<pair<string,long double> >::iterator listIt;
  list<pair<string,long double> > wordList; //list of words, to be ordered by probability
  list<pair<string,long double> > outputList; 

  if(wordIndexTable.empty()){
    cout << "ERROR wordIndexTable empty in initMeans(), init failed" << endl;
    return;
  }

  means.resize(k);
  if(!means.empty()){
    means.clear();
  }

  //get a list of all the unique words
  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    pair<string,long double> p;
    p.first = it->first;
    p.second = it->second.second.size();
    wordList.push_front(p);
  }

  //now grab k random means from the output list of top frequency words
  i = 0;
  while(i < k){ //"l" condition just prevents an infinite loop
    r = rand() % wordList.size();
    //spool to index r (list doesn't support [] operator)
    listIt = wordList.begin();
    for(j = 0; (j < r) && (listIt != wordList.end()); j++){
      listIt++;
    }

    if(listIt != wordList.end() && !hasMean(listIt->first,means)){ //filter dupes
      means.push_back(listIt->first);
      i++;
    }
  }
}

/*
  Init the means vector to the entire vocabulary
  This allows the k-means update assignments to do the clustering
  for us, rather than fixing the cluster size.

*/
void kNN::initMaxMeans(vector<string>& means)
{
  IndexTableIt it;

  if(wordIndexTable.empty()){
    cout << "ERROR wordIndexTable empty in initMeans(), init failed" << endl;
    return;
  }

  means.resize(wordIndexTable.size());
  if(!means.empty()){
    means.clear();
  }

  //inits means to the entire vocabulary
  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    means.push_back(it->first);
  }
}

/*
  Many of these patterns are pretty lazy, for getting some random k words in the top 200 wordset.
*/
void kNN::initMeans(const int k, vector<string>& means)
{
  int i, j, r, l;
  IndexTableIt it;
  list<pair<string,long double> >::iterator listIt;
  list<pair<string,long double> > wordList; //list of words, to be ordered by probability
  list<pair<string,long double> > outputList; 

  if(wordIndexTable.empty()){
    cout << "ERROR wordIndexTable empty in initMeans(), init failed" << endl;
    return;
  }

  means.resize(k);
  if(!means.empty()){
    means.clear();
  }

  //get a list of all the unique words
  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    pair<string,long double> p;
    p.first = it->first;
    p.second = it->second.second.size();
    wordList.push_front(p);
  }
  wordList.sort(byFrequency);
  //dbg. make sure the list is in descending order
/*
  i = 0;
  for(listIt = wordList.begin(); (i < 150) && (listIt != wordList.end()); ++listIt){
    cout << (i++) << ": " << listIt->first << "|" << listIt->second << endl;
  }
*/
  //omit all the items > than index 200 from output list
  for(i = 0, listIt = wordList.begin(); (i < 2000) && (listIt != wordList.end()); ++listIt, i++){
    outputList.push_back(*listIt);
  }
  wordList.clear(); //done with word list
/*
  for(listIt = outputList.begin(); (i < MAX_RAND_MEANS) && (listIt != outputList.end()); ++listIt){
    cout << (i++) << ": " << listIt->first << "|" << listIt->second << endl;
  }
*/

  //now grab k random means from the output list of top frequency words
  for(i = 0, l = 0; (i < k) && (l < (k << 2)) && (i < outputList.size()); l++){ //"l" condition just prevents an infinite loop
    r = rand() % outputList.size();
    //spool to index k (list doesn't support [] operator)
    listIt = outputList.begin();
    for(j = 0; (j < r) && (listIt != outputList.end()); j++){
      listIt++;
    }
    if(!hasMean(listIt->first,means)){ //filter dupes
      means.push_back(listIt->first);
      i++;
    }
  }

  //dbg
  cout << "the random means (k= " << k << " size=" << means.size() << "): " << endl;
  for(i = 0; i < means.size(); i++){
    cout << means[i] << " ";
  }
  cout << endl;
}

/*
  Given some word, find the word in means vector that it is nearest. If none is found,
  returns "NIL"

  This is the hard-max version, similar to Mackay "Info Theory..." p.286 (except that maximum
  similarity measures "min-distance"). We "hard" assign words to the mean with which they share
  the greatest degree of similarity.
*/
string kNN::findNearestNeighbor_HardMax(const string& word, const vector<string>& means)
{
  U32 i;
  long double temp, max;
  string ret_s = "NIL";
  
  for(max = 0, i = 0; i < means.size(); i++){
    temp = getSimilarity(word, means[i]);
    if(temp > max){
      max = temp;
      ret_s = means[i];
    }
    //dbg

    if(temp <= 0){
      //cout << "WARN temp <= 0 in findNEarestNeighbor(w1,w2). temp=" << temp << endl;
    }
  }

  return ret_s;
}

/*
  Given some word, find the word in means vector that it is nearest. If none is found,
  returns "NIL"

  This is the hard-max version, similar to Mackay "Info Theory..." p.286 (except that maximum
  similarity measures "min-distance"). We still assign words to the mean with which they share
  the greatest degree of similarity, but this degree of similarity is "softened" by dividing it
  by the sum similarity of that word to all other words:

  similarity = directSimilarity(w1,w2) / sumSimilarity(w1), where w1 is some word, w2 is some mean

  Intent of the 1/sumSimilarity proportion is to not let very common words dominates the means

*/
string kNN::findNearestNeighbor_SoftMax(const string& word, const vector<string>& means)
{
  U32 i;
  long double temp, max;
  string ret_s = "NIL";
  
  for(max = 0, i = 0; i < means.size(); i++){
    temp = getSimilarity(word, means[i]) / getSumSimilarity(means[i]);
    if(temp > max){
      max = temp;
      ret_s = means[i];
    }
    //dbg
    if(temp <= 0){
      cout << "WARN temp <= 0 in findNEarestNeighbor(w1,w2). temp=" << temp << endl;
    }
  }

  return ret_s;
}

//instead of only finding the nearest neighbor (some argmax() val), finds some set of top-ranked elements
void kNN::findTopNeighbors(const string& word, const vector<string>& means, list<pair<string,double> >& neighbors)
{
  U32 i;
  pair<string,double> p, max1, max2, max3;

  max1.second = max2.second = max3.second = -1.0;
  //just use multimaxes if selecting top k < 5 or so, instead of sorting or insert sorting  
  for(i = 0; i < means.size(); i++){
/*
p.second = getSimilarity(word, means[i]) / getSumSimilarity(means[i]);
p.first = means[i];
neighbors.push_front(p);
*/

    p.second = getSimilarity(word, means[i]);

    //p.second = getSimilarity(word, means[i]) / getSumSimilarity(means[i]);
    p.first = means[i];
    if(p.second > max1.second){
      max1 = p;
    }
    if(p.second < max1.second && p.second > max2.second){
      max2 = p;
    }
    if(p.second < max2.second && p.second > max3.second){
      max3 = p;
    }
  }
  neighbors.push_front(max1);
  neighbors.push_front(max2);
  neighbors.push_front(max3);



//neighbors.sort(bySimilarity);

  /*
  Top ranked could be some threshold like 0.05, or maybe a proportion of the size of means (careful of the fact that this size shrinks)
  */

  //dbg
/*
  cout << "Top neighbors for word >" << word << "<:" << endl;
  for(list<pair<string,double> >::iterator it = neighbors.begin(); it != neighbors.end(); ++it){
    cout << it->first << "|" << it->second << endl;
  }
*/
}

/*
  Returns sum similarity of w1 to all other words. This provides a damping proportion for very common words.
  Its an experimental question whether or not using this to handicap such terms is a good idea, per clustering,
  or if clustering should be allowed to favor common words. IOW, I have no idea if this is mathematically valid,
  and it needs to be proven mathematically sound.
*/
FloatType kNN::getSumSimilarity(const string& w1)
{
  char c;
  U16 key;
  double sum; //return 1.0 by default, to prevent division by zero in findNearest()
  CoDistOuterIt outer;
  CoDistInnerIt inner;

  key = strToId(w1);
  if(key == 0){
    cout << "ERROR key==0 for word=" << w1 << " in getSumSimilarity()" << endl;
    return 1.0;
  }
  outer = CoDistTable.find(key);
  if(outer == CoDistTable.end()){
    cout << "ERROR key found, but w1 not found in CoDistTable: " << w1 << endl;
    return 1.0;
  }

  for(sum = 0.0, inner = outer->second.begin(); inner != outer->second.end(); ++inner){
    sum += inner->second;
    //cout << "sum is: " << sum << endl;
    //cin >> c;
  }
  //cout << "sum [" << w1 << "] is: " << sum << endl;

  if(sum <= 0.0){
    cout << "ERROR sum <= 0 in getSumSimilarty. sum=" << sum << endl;
    sum = 1.0;
  }

  return (long double)sum;
}

/*
  Just another exception-case/single-context function. Builds a word index table based off
  CoDistanceTable keys. This means the CoDistanceTable is in memory, but for some reason
  the wordIndexTable isn't (such as if we rebuilt from a file).
  Side-effects may include that the wordIndexTable therefore only contains the set of items
  that are in the CoDistanceTable, but this is a condition we try to satisfy anyway. Also
  you won't have the word indices for a given word, every word will just have a single item
  vector containing integer-index 0.
*/
void kNN::buildDummyWordIndexTable(void)
{
  string s;
  CoDistOuterIt outer;
  CoDistInnerIt inner;

  for(outer = CoDistTable.begin(); outer != CoDistTable.end(); ++outer){
    s = idToStr(outer->first);
    if(wordIndexTable.find(s) == wordIndexTable.end()){
      wordIndexTable[s].second.push_back(0);
    }
    for(inner = outer->second.begin(); inner != outer->second.end(); ++inner){
      s = idToStr(inner->first);
      if(wordIndexTable.find(s) == wordIndexTable.end()){
        wordIndexTable[s].second.push_back(0);
      }
    }
  }
}

void kNN::writeMeans(int fd)
{
  U32 i;
  string ostr;

  //append current means to file
  if(fd > 0){
    for(i = 0; i < wordMeans.size(); i++){
      ostr += wordMeans[i];
      ostr += " ";
      if(i % 10 == 9){ ostr += "\n"; }
    }
    ostr += "\n\n";
    write(fd,ostr.c_str(),ostr.size());
  }
}


/*
  Given some k, assign k random means. To increase the likelihood of these means
  having sufficient neighbors, these means are chosen from the top 20% most likely
  terms. This is to make sure these terms have the most relationships (graph edges) between them.

  Pre-condition: CoDistance table, and 1-gram word probability tables must exist in memory.

  Convergence condition: test for n iterations, test when means set begins to stay the same (say,
  greater than 80% of the means stay the same on each iteration), or when means begin to stay the same
  AND the size of their respective wordsets no longer oscillates much.

  Motivation behind clustering is semantic, but think about it causally. Clustering implies words arise from a proximate cause (clusters of subjects).
  But obviously most words belong to multiple clusters, but to varying degrees.

*/
void kNN::kMeansSoft(int k)
{
  int ofile;
  U32 i, j, m, q, r;
  long double sum, sumsim, max;
  const string nil = "NIL";
  string newMean, s;
  IndexTableIt it;
  list<pair<string,double> > neighbors;
  list<pair<string,double> >::iterator lit;
  map<string,vector<string> >::iterator subIt;

  if(CoDistTable.empty()){
    cout << "CoDistTable[][] empty in kMeans, returning" << endl;
    return;
  }
  ofile = open(meansFile.c_str(), O_WRONLY | O_CREAT);
  if(ofile < 0){
    cout << "ERROR ./means.txt could not be opened for write" << endl;
  }
  if(wordIndexTable.empty()){
    cout << "wordIndexTable[] empty in kMeans, building new unique word table" << endl;
    //rebuild the word index with only words (no indexes)    
    buildDummyWordIndexTable();
  }

  cout << "beginning softkmeans..." << endl;
  //until convergence, do:  
  // 1) partition word set by means
  // 2) find new set of means within each set
//initMeans(k, wordMeans);  //inits means to random k selected from top 200 words. clears and resizes wordMeans to k-size
  initMaxMeans(wordMeans);
  for(q = 0; q < 20; q++){

    //append current means to file
    writeMeans(ofile);

    // Cluster words about the means~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      //first clear each cluster's word subset
    if(!kSubsets.empty()){
      kSubsets.clear();
    }

      //assign each word to the set with the nearest mean
    for(i = 0, it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it, i++){
      //for k very small (very sparse co-distance matrices) this will often return "NIL"
      //get a list of the top "n" neighbors depending on what their distribution looks like...
      findTopNeighbors(it->first,wordMeans,neighbors);
      //adds this word to multiple sets (some top-ranked set of n-neighbors)
      for(lit = neighbors.begin(); lit != neighbors.end(); ++lit){
        kSubsets[ lit->first ].push_back(it->first);
      }
      neighbors.clear();
      if((i % 100) == 0){
        cout << "\r" << i << " of " << wordIndexTable.size() << " words assigned to means" << flush;  
      }
    }
    cout << "sizeof unknown words for this set of initial random  means: " << kSubsets[nil].size() << endl;
/*
    cout << "sizeof unknown words for this set of initial random  means: " << kSubsets[nil].size() << endl;
    for(subIt = kSubsets.begin(); subIt != kSubsets.end(); ++subIt){
      cout << subIt->first << "|" << subIt->second.size() << endl;
    }
*/
    //Find the new means within the current clusters~~~~~~~~~~~~~~~~~~~
    //now find the "centroid" within each set of words, building the next set of means
    //iterating the sets: O(k * n^2)
    wordMeans.clear();
    for(m = 0, subIt = kSubsets.begin(); subIt != kSubsets.end(); ++subIt, m++){
      //sum each word's proximity to every other word, then take the max. this is a somewhat naive update.
      // the update could also incorporate relative probabilities into each sum, or other more advanced formulae
      for(newMean = nil, max = 0, i = 0; i < subIt->second.size(); i++){
        //get this word's sum-relatedness to every other word in the list
      //sumsim = getSumSimilarity(subIt->second[i]); //proportioning constant
        for(sum = 0.0, j = 0; j < subIt->second.size(); j++){
          if(i != j){
      //      sum += (getSimilarity(subIt->second[i],subIt->second[j]) / sumsim);
              sum += sqrt(getSimilarity(subIt->second[i],subIt->second[j]));  //sqrt just like linear distance, or sum squared error
          }
        }
        if(sum > max){
          max = sum;
          newMean = subIt->second[i];
        }
        //cout << "working inner... i=" << i << " j=" << j << " subIt->first=" << subIt->first << endl;
      }
      //verify new mean is both not NIL and not already in the vector of means
      //if((newMean != nil) && (!hasMean(newMean,wordMeans))){
      if(newMean != nil){
        //wordMeans[m++] = newMean;
        wordMeans.push_back(newMean);
      }
      cout << "\rworking outer... m=" << m << " of " << kSubsets.size() << flush;
    }
    cout << endl;

    //run until no changes in means, then iteratively re-run to find the most frequently-occurring means
    cout << "iteration "<< q << ": ";
    printMeans(wordMeans);
  }

  writeClusters(kSubsets,wordMeans);

  if(ofile > 0){ close(ofile); }
}

/*
  Given some k, assign k random means. To increase the likelihood of these means
  having sufficient neighbors, these means are chosen from the top 20% most likely
  terms. This is to makeane sure these terms have the most relationships (graph edges) between them.

  This algorithm is different from k-means since k varies according to which mean is nearest to each
  word in the vocabulary. See wordIndexTable iteration for code example. It assigns each word in the
  vocabulary arbitrarily to the nearest means, but this allows these sets to expand/contract and some
  means to be lost (if no word is nearer to them than the others).

  Its interesting, and demonstrates how the tuning params of k-means create different set profiles.


  Pre-condition: CoDistance table, and 1-gram word probability tables must exist in memory.
*/
void kNN::cosineClustering(int k)
{
  int ofile;
  U32 i, j, m, q;
  long double sum, max;
  const string nil = "NIL";
  string newMean, s;
  IndexTableIt it;
  map<string,vector<string> >::iterator subIt;

  if(CoDistTable.empty()){
    cout << "CoDistTable[][] empty in kMeans, returning" << endl;
    return;
  }
  ofile = open(meansFile.c_str(), O_WRONLY | O_CREAT);
  if(ofile < 0){
    cout << "ERROR ./means.txt could not be opened for write" << endl;
  }
  if(wordIndexTable.empty()){
    cout << "WARN wordIndexTable[] empty in kMeans, building new unique word table" << endl;
    //rebuild the word index with only words (no indexes)    
    buildDummyWordIndexTable();
  }

/*
  cout << "in kmeans, indexTable.size()=" << wordIndexTable.size() << endl;
  if(wordIndexTable.size() > 10000){
    pruneWordIndexTable(wordIndexTable);
  }
  cout << "after prune in kmeans, indexTable.size()=" << wordIndexTable.size() << endl;
*/

  /*
  //initial state, partition according to the random means among the most frequent (top k+200) terms
  //for each mean, partition the entire set of words according to which mean they are nearest
  for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
    //for k very small (very sparse co-distance matrices) this will often return "UNK"
    kSubsets[ findNearestNeighbor(it->first,means) ].push_back(it->first);    
  }
  */

  //initMeans(k, wordMeans);  //inits means to random k selected from top 200 words. clears and resizes wordMeans to k-size
  //initRandomMeans(k, wordMeans);
  initMaxMeans(wordMeans);
  cout << "beginning hardkmeans, k=" << k << " and means.size()=" << wordMeans.size() << " ..." << endl;

  //until convergence, do:  
  // 1) partition word set by means
  // 2) find new set of means within each set
  for(q = 0; q < 20; q++){

    //append current means to file
    writeMeans(ofile);

    // Cluster words about the means~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      //first clear each cluster's word subset
    if(!kSubsets.empty()){
      kSubsets.clear();
    }
    /*
    for(subIt = kSubsets.begin(); subIt != kSubsets.end(); ++subIt){
      //for k very small (very sparse co-distance matrices) this will often return "NIL"
      subIt->second.clear();
    }
    */
      //assign each word to the set with the nearest mean
      //This is hard clustering, since each word is assigned to one and only one mean. See Mackay "Info theory..." p.286 for info
    for(it = wordIndexTable.begin(); it != wordIndexTable.end(); ++it){
      //for k very small (very sparse co-distance matrices) this will often return "NIL"
      kSubsets[ findNearestNeighbor_HardMax(it->first,wordMeans) ].push_back(it->first);
    }
    cout << "sizeof unknown words for this set of initial random  means: " << kSubsets[nil].size() << endl;
/*
    cout << "sizeof unknown words for this set of initial random  means: " << kSubsets[nil].size() << endl;
    for(subIt = kSubsets.begin(); subIt != kSubsets.end(); ++subIt){
      cout << subIt->first << "|" << subIt->second.size() << endl;
    }
*/
    //Find the new means within the current clusters~~~~~~~~~~~~~~~~~~~
    //now find the "centroid" within each set of words, building the next set of means
    //iterating the sets: O(k * n^2)
    wordMeans.clear();
    for(m = 0, subIt = kSubsets.begin(); subIt != kSubsets.end(); ++subIt){
      //sum each word's proximity to every other word, then take the max. this is a somewhat naive update.
      // the update could also incorporate relative probabilities into each sum, or other more advanced formulae
      for(newMean = nil, max = 0, i = 0; i < subIt->second.size(); i++){
        //get this word's sum-relatedness to every other word in the list
        for(sum = 0, j = 0; j < subIt->second.size(); j++){
          if(i != j){
            sum += sqrt(getSimilarity(subIt->second[i],subIt->second[j]));
          }
        }
        if(sum > max){
          max = sum;
          newMean = subIt->second[i];
        }
        //cout << "working inner... i=" << i << " j=" << j << " subIt->first=" << subIt->first << endl;
      }
      //verify new mean is both not NIL and not already in the vector of means
      //if((newMean != nil) && (!hasMean(newMean,wordMeans))){
      if(newMean != nil){
        //wordMeans[m++] = newMean;
        wordMeans.push_back(newMean);
      }
      cout << "\rworking outer... m=" << m << " of " << wordMeans.size() << flush;
    }
    cout << endl;

    //run until no changes in means, then iteratively re-run to find the most frequently-occurring means
    cout << "iteration "<< q << ": ";
    printMeans(wordMeans);
  }

  writeClusters(kSubsets,wordMeans);

  if(ofile > 0){ close(ofile); }
}

void kNN::writeClusters(map<string, vector<string> >& clusters, vector<string>& means)
{
  U32 i;
  int ofile;
  string ostr;
  map<string,vector<string> >::iterator it;

  ofile = open(kMeansFile.c_str(),O_WRONLY);
  if(ofile < 0){
    cout << "ERROR could not open kMeans.txt for write" << endl;
    return;
  }
  ostr.reserve(1 << 12);

  ostr = "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
  ostr += std::to_string(clusters.size());
  ostr += " means--\n    ";
  for(i = 0; i < means.size(); i++){
    ostr += means[i];
    ostr += " ";
    if((i % 12) == 11){
      ostr += "\n    ";
    }
  }
  ostr += "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n";
  write(ofile,ostr.c_str(),ostr.size());
  ostr.clear();

  for(it = clusters.begin(); it != clusters.end(); ++it){
    ostr = it->first;
    ostr += ":\n    ";
    for(i = 0; i < it->second.size(); i++){
      ostr += it->second[i];
      ostr += " ";
      if((i % 12) == 11){
        ostr += "\n    ";
      }
    }
    ostr += "\n";
    write(ofile,ostr.c_str(),ostr.size());
    ostr.clear();    
  }

/*  for(i = 0; i < means.size(); i++){
    subIt = clusters.find(means[i]);

    if(subIt != clusters.end()){
      ostr = means[i];
      ostr += ":\n    ";    
      for(j = 0; j < subIt->second.size(); j++){
        ostr += subIt->second[j];
        ostr += " ";
        if((j % 12) == 11){
          ostr += "\n    ";
        }
      }
      ostr += "\n";
      write(ofile,ostr.c_str(),ostr.size());
      ostr.clear();
    }
  }
*/

  close(ofile);
}

bool kNN::hasMean(const string& word, const vector<string>& means)
{
  for(int i = 0; i < means.size(); i++){
    if(word == means[i]){
      return true;
    }
  }
  return false;
}

void kNN::printMeans(const vector<string>& means)
{
  string ostr;

  cout << "the " << means.size() << " means: " << endl;
  for(int i = 0; i < means.size(); i++){
    cout << means[i] << " ";
    if((i % 10) == 9)
      cout << "\n";
  }
  cout << endl;
}



















