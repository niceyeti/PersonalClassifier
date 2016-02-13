#include "TextProcessor.hpp"


TextProcessor::TextProcessor()
{
  cout << "ERROR TextProcessor default ctor called. Use ctor with stopWord filename parameter instead" << endl;

  phraseDelimiters = "\".?!#;:)(";  // octothorpe is user defined
  rawDelimiters = "\"?!#;:)(, "; //all but period
  wordDelimiters = ", ";
  delimiters = phraseDelimiters;  //other special chars? could be useful for technical texts, eg, financial reports
  delimiters += wordDelimiters;
  //delimiters += "'";
  wordDelimiter = ' ';
  phraseDelimiter = '#';

  initValidDecimalChars();
  string s = "../stopWords1.txt";
  //string s = "../stopWordsExtended.txt";
  buildStopWordTable(s);
}

TextProcessor::TextProcessor(const string& stopWordFile)
{
  phraseDelimiters = "\".?!#;:)(";  // octothorpe is user defined
  rawDelimiters = "\"?!#;:)(, "; //all but period
  wordDelimiters = ", ";
  delimiters = phraseDelimiters;  //other special chars? could be useful for technical texts, eg, financial reports
  delimiters += wordDelimiters;
  //delimiters += "'";
  wordDelimiter = ' ';
  phraseDelimiter = '#';

  initValidDecimalChars();
  //string s = "../stopWordsExtended.txt";
  buildStopWordTable(stopWordFile);
}

TextProcessor::~TextProcessor()
{
  m_stopWords.clear();
}

/*
  Converts a text file to a gigantic string of words. Stopwords and invalid words are dropped and text is normalized.
*/
void TextProcessor::textToWordSequence(const string& trainingFile, vector<string>& wordSequence, bool filterStopWords)
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
  if(m_stopWords.size() == 0){
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
../../stopWordsLong.txt

*/
void TextProcessor::buildStopWordTable(const string& stopWordFile)
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
    if(m_stopWords.count(s) == 0){
      m_stopWords.insert(s);
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

bool TextProcessor::isStopWord(const char* word){
  string s = word;
  return (m_stopWords.count(s) > 0);
}

bool TextProcessor::isStopWord(const string& word){
  return (m_stopWords.count(word) > 0);
}

/*
  overload to support char*
*/
bool TextProcessor::isValidWordExtended(const char* word)
{
  string w = word;
  return isValidWordExtended(w);
}

/*
  Small utility for finding the position of a char in some input buffer.
  Used when we need to lseek() back to some position in a file to verify we're
  reading delimiter-length segments.

  Returns: position of char c, or -1 if not in string.
*/
int TextProcessor::findChar(const char buf[], char c, int len)
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
int TextProcessor::seekLastPhraseDelimiter(const char buf[BUFSIZE], int len)
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

bool TextProcessor::isPhraseDelimiter(char c)
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
void TextProcessor::normalizeText(char ibuf[BUFSIZE], string& ostr)
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
void TextProcessor::delimitText(string& istr)
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

bool TextProcessor::isWordDelimiter(char c)
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
void TextProcessor::scrubHyphens(string& istr)
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
void TextProcessor::finalPass(string& buf)
{
  for(int i = 0; i < buf.length(); i++){
    if(buf[i] == PERIOD_HOLDER){
      buf[i] = '.';
    }
  }
}

void TextProcessor::toLower(string& myStr)
{
  for(int i = 0; i < myStr.length(); i++){
    if((myStr[i] >= 'A') && (myStr[i] <= 'Z')){
      myStr[i] += 32;
    }
  }
}

//standardize input by converting to lowercase
void TextProcessor::toLower(char buf[BUFSIZE])
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
void TextProcessor::rawPass(string& istr)
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
bool TextProcessor::isDelimiter(const char c, const string& delims)
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
int TextProcessor::tokenize(char* ptrs[], char buf[BUFSIZE], const string& delims)
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


bool TextProcessor::isInt(const string& nstr)
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
bool TextProcessor::isDecimal(const string& nstr)
{
  for(int i = 0; i < nstr.size(); i++){
    if(!validDecimalChars[(int)nstr[i]]){
      return false;
    }
  }
  return true;
}

bool TextProcessor::hasColon(const char buf[], int len)
{
  for(int i = 0; buf[i] != '\0' && i < len; i++){
    if(buf[i] == ':'){
      return true;
    }
  }
  return false;
}


void TextProcessor::initValidDecimalChars(void)
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
  Return true if token contains only numbers or slashes (such as date strings),
  no dollar signs, ampersands, colons, @, etc
  
  Excludes any words containing: 0123456789@:;<=>?#$%&[]^`_
*/
bool TextProcessor::isValidWordExtended(const string& token)
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
bool TextProcessor::isValidWord(const char* word)
{
  string w = word;
  return isValidWord(w);
}

bool TextProcessor::isValidWord(const string& token)
{
  bool ret = true;

  //no words longer than limit
  if(token.length() > MAX_WORD_LEN){
    //cout << "\rWARN unusual length word in isValidWord: >" << token << "< ignored. Check parsing                        " << endl;
    ret = false;
  }

  if(token[0] == '\''){  // filters much slang: 'ole, 'll, 'em, etc
    ret = false;
  }
  if((token[0] == '*') || (token[1] == '*')){
    ret = false;
  }
  if(token == "com"){  //lots of "www" and ".com" etc
    //cout << "invalid word: " << token << endl;
    ret = false;
  }
  if(token == "www"){  //lots of "www"
    //cout << "invalid word: " << token << endl;
    ret = false;
  }
  if(token == "http"){  //lots of "http"
    //cout << "invalid word: " << token << endl;
    ret = false;
  }

  if((token[0] == '\'') && ((token[1] == '\'') || (token[1] == 's'))){  //hack: covers '' and 's in output stream
    ret = false;
  }
  if(token == "th"){  //occurs when "8th" is converted to "th" after numeric drop
    ret = false;
  }

  for(U32 i = 0; i < token.length(); i++){
    if((token[i] >= 47) && (token[i] <= 64)){ ///exclude all of 0123456789@:;<=>?
      //cout << "invalid word: " << token << endl;
      ret = false;
    }
    if((token[i] >= 35) && (token[i] <= 38)){ ///exclude all of #$%&
      //cout << "invalid word: " << token << endl;
      ret = false;
    }
    if((token[i] >= 91) && (token[i] <= 96)){ ///exclude all of []^`_
      //cout << "invalid word: " << token << endl;
      ret = false;
    }
  }

  return ret;
}



