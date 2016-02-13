#include "PersonalClassifier.hpp"

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

PersonalClassifier::PersonalClassifier()
{
  cout << "ERROR PersonalClassifier default ctor called; do not use. Use the ctor with file params..." << endl;
  exit(0);
}

PersonalClassifier::PersonalClassifier(const string& stopWordFile)
{
  //cout << "Constructing PersonalClassifier..." << endl;
  m_wordIdManager = new WordIdManager();
  m_textProcessor = new TextProcessor(stopWordFile);
}


PersonalClassifier::~PersonalClassifier()
{
  delete m_wordIdManager;
  delete m_textProcessor;
}

/*
  This is generalized to both generate personal models and test sample models. So given
  a wordstream, this will produce a similarity matrix, no matter the actual size of the input
  (small for samples, very large for personal training data).
*/
void PersonalClassifier::buildSimilarityModel(const string& trainingFile, Matrix& simMatrix)
{
  vector<string> wordStream;
  WordIndexTable wordIndexTable;
  Matrix coOccurrenceMatrix; //intermediate matrix for storing co-occurrence data, which is then transformed and stored in simMatrix

  if(!simMatrix.empty()){
    cout << "WARN non-empty simMatrix passed to BuildSimilarity model, clearing matrix..." << endl;
    simMatrix.clear();
  }

  cout << "Building similarity model..." << endl;
  m_textProcessor->textToWordSequence(trainingFile,wordStream,true);
  buildWordIndexTable(wordStream,wordIndexTable);
  buildCoOccurrenceMatrix(wordStream,wordIndexTable,coOccurrenceMatrix);
    wordIndexTable.clear(); //free the wordIndex table and wordStream mem
    wordStream.clear();
  cout << "build codistmatrix complete, now building similarity matrix..." << endl;
  buildSimilarityMatrix(coOccurrenceMatrix,simMatrix);
  string ofile = "../matrix.txt";
  matrixToFile(ofile,simMatrix);
  cout << "complete" << endl;
}

void PersonalClassifier::matrixToFile(string& fname, const Matrix& matrix)
{
  int ofile, i;
  string ostr;
  ConstMatrixOuterIt outer;
  ConstMatrixInnerIt inner;
  list<string> olist;

  if(matrix.empty()){
    cout << "ERROR matrix empty in MatrixToFile()" << endl;
    return;
  }
  ofile = open(fname.c_str(),O_WRONLY | O_CREAT | O_TRUNC);
  if(ofile < 0){
    cout << "ERROR matrixToFile() could not open file >" << fname << "<" << endl;
    return;
  }

  cout << "writing matrix to file: " << fname << endl;

  i = 0;
  ostr.reserve(1 << 12);
  //build a very large list of all the records in the coDistTable, as strings. then sort and output to file.
  for(outer = matrix.cbegin(); outer != matrix.end(); ++outer){
    for(inner = outer->second.cbegin(); inner != outer->second.end(); ++inner){
      ostr += m_wordIdManager->idToString(outer->first);
      ostr += "\t";
      ostr += m_wordIdManager->idToString(inner->first);
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
  cout << "write complete. num uniq keys in matrix: " << i << endl; 

  close(ofile);
}

/*
  Core mathematical analysis driver of the program: take dot product of input sample and some personal model.
  This is simply the dot product of two 2d matrices, whose output vector is therefore a 1d vector. How this
  vector will be used for comparisons I don't know; I'll figure it out later, either by "scalarizing" it, or
  correlating over its attributes, perhaps by linear interpolation, whereby less frequent signature terms could be 
  given greater weight.

  Method: We assume one of these matrices will be quite small. Both will be incredibly sparse, but one will likely be an
  input sample of only a few hundred words. Since both are sparse, iterate over only the common terms, else assume they are
  zero (eg, don't iterate the entire matrices, as a formal dot product). Further, project the smaller matrix's terms into the
  larger one, which takes far fewer lookups. This gives the precise number of necessary lookups, instead of doing a ton of them.
*/
void PersonalClassifier::matrixDotProduct(Matrix& largerMatrix, Matrix& smallerMatrix, DotProductVector& resultant)
{
  double sum;
  MatrixOuterIt largeOuterIt, smallOuterIt;
  MatrixInnerIt largeInnerIt, smallInnerIt;

  if(!resultant.empty()){
    cout << "WARN resultant not empty in DotProduct, clearing vector..." << endl;
    resultant.clear();
  }

  //iterate the smaller matrix looking for its entries in the larger matrix
  for(smallOuterIt = smallerMatrix.begin(); smallOuterIt != smallerMatrix.end(); ++smallOuterIt){

    //if larger matrix contains this term, iterate over the vectors (columns) looking for smaller matrix entries in the larger, summing the products
    largeOuterIt = largerMatrix.find(smallOuterIt->first);
    if(largeOuterIt != largerMatrix.end()){
      //larger matrix contains (a cohttp://www.weather.com/weather/tenday/l/Pullman+WA+99163:4:USlumn for) this term, so iterate vectors in parallel summing products
      sum = 0.0;
      for(smallInnerIt = smallOuterIt->second.begin(); smallInnerIt != smallOuterIt->second.end(); ++smallInnerIt){
        largeInnerIt = largeOuterIt->second.find(smallInnerIt->first);
        if(largeInnerIt != largeOuterIt->second.end()){
          sum += (largeInnerIt->second * smallInnerIt->second); //sum the products of common terms for these columns
        }
      }

      if(sum > 0.0){
        resultant.push_back( ResultantEntry(largeOuterIt->first, sum) );
      }
    }
  }

  //cout << "Dot product completed, vec size is: " << resultant.size() << " shared terms" << endl;
  //printDotProductVector(resultant);
}

//Prints a 1-d resultant vector
void PersonalClassifier::printDotProductVector(const DotProductVector& vec)
{
  for(int i = 0; i < vec.size(); i++){
    cout << "  " << m_wordIdManager->idToString(vec[i].first) << ":" << vec[i].first << "  " << vec[i].second << "\n";
  }
  cout << endl;
}

/*
  increment a co-location count for two words, key1 and key2.
  This function is meant to implicitly filter dupe key permutations from the table.
*/
void PersonalClassifier::updateCoOccurrenceMatrix(const string& key1, const string& key2, Matrix& coOccurrenceMatrix)
{
  U32 k1, k2;
  bool foundK1, foundK2;
  MatrixOuterIt outer;
  MatrixInnerIt inner;

  foundK1 = foundK2 = false;

  k1 = m_wordIdManager->stringToId(key1);
  k2 = m_wordIdManager->stringToId(key2);
  if(k1 <= 0){
    cout << "ERROR k1=0 for key1=" << key1 << " in updateCoOccurrenceMatrix" << endl;  
    return;
  }
  if(k2 <= 0){
    cout << "ERROR k2=0 for key2=" << key2 << " in updateCoOccurrenceMatrix" << endl;
    return;
  }

/*
  if(coOccurrenceMatrix.empty()){ //logic exception
    CoOccMatrix[k1][k2] = 1.0;
    return;
  }
*/

  //find first entry for this pair at [k1][k2] entry
  outer = coOccurrenceMatrix.find(k1);
  if(outer != coOccurrenceMatrix.end()){
    inner = outer->second.find(k2);
    if(inner != outer->second.end()){
      inner->second++;
      //return;
      foundK1 = true;
    }
  }

  //find second entry for this pair at [k2][k1] entry
  outer = coOccurrenceMatrix.find(k2);
  if(outer != coOccurrenceMatrix.end()){
    inner = outer->second.find(k1);
    if(inner != outer->second.end()){
      inner->second++;
      //return;
      foundK2 = true;
    }
  }

  if(foundK1 ^ foundK2){  //XOR: only one of the two is found
    cout << "ERROR only one of k1 or k2 found in updateCoOccurrence coOccurrenceMatrix; keys misaligned! If one exists, both must, for symmetry." << endl;
  }

  if(!foundK1 && !foundK2){
    //only reach here if neither key1+key and key2+key1 found in table
    coOccurrenceMatrix[k1][k2] = 1.0;
    coOccurrenceMatrix[k2][k1] = 1.0;
  }
}


//
void PersonalClassifier::testPerson(const string& personalTrainingFile, const string& personalSampleFile)
{
  Matrix trainingMatrix, testMatrix;
  DotProductVector dotProduct;

  //build the personally-trained matrix
  buildSimilarityModel(personalTrainingFile, trainingMatrix);
  //build the matrix for the test sample
  buildSimilarityModel(personalSampleFile, testMatrix);
  //take the dot product of both to get the classifier
  matrixDotProduct(trainingMatrix,testMatrix,dotProduct);
  printDotProductVector(dotProduct);
  cout << "testPerson() completed for " << personalTrainingFile << endl;
}

/*
  Note the pre/post conditions of this function carefully, especially wrt the wordIndexTable and the wordSequence

  Init state: let wordSequence and wordIndexTable be raw, unpruned, though based off one another.
  Post-state: CoOccMatrix is built, wordIndexTable pruned, and the vocabulary of the two is 1:1 (aligned)

  wordSequence is DESTROYED! to reduce high-memory consumption. Just be aware.
*/
//void PersonalClassifier::buildCoOccurrenceMatrix(Matrix& matrix, vector<string>& wordSequence)
void PersonalClassifier::buildCoOccurrenceMatrix(vector<string>& wordSequence, WordIndexTable& wordIndexTable, Matrix& matrix)
{
  U32 i, ct, radius, left, pivot, right;
  WordIndexTableIt outer;  //iterates over each word in the map
  MatrixOuterIt sim_outer;
  MatrixInnerIt sim_inner;

  if(wordSequence.empty()){
    cout << "ERROR wordSequence empty in buildCoOccurrenceMatrix, build failed" << endl;
    return;
  }

  //remember the distribution of occurrence list lengths is very important. usually these are very bottom heavy, and the average is thrown by large outliers
  //cout << "wordIndexTable.size()=" << wordIndexTable.size() << " maxLen=" << maxlen << endl;
  cout << "building CoOccurrence table from wordIndexTable keys related over filtered word sequence.\n"
       << "NOTE if wordIndexTable has been pruned, CoOccMatrix keys will represent only a relatively\n"
       << "small (~10-15%) subset of the raw word sequence. Also note how distance vals imply a\n"
       << "probabilistic scalar between terms..." << endl;

  //Correlate each word in the sequence with every other word in the sequence
  radius = (U32)CODIST_DIAMETER >> 1;
  cout << "building with radius=" << radius << endl;
  ct = 0;
  //for each word index, look *radius* words back and forward to find related words
  for(outer = wordIndexTable.begin(); outer != wordIndexTable.end(); ++outer){
    //iterate over the word indices: [34,56,234,345] for this word
    for(i = 0; i < outer->second.size(); i++){
    
      //get the word index bounds for the evaluation window of *radius* words to left and right
      pivot = outer->second[i];
      if(radius >= pivot){  //same as argmax(pivot-radius,0). This is easier for unsigned.
        left = 0;
      }
      else{
        left = pivot - radius;
      }
      right = argmin(pivot+radius,(U32)wordSequence.size());
    
      if(left >= right){
        cout << "WARN left=" << left << " >= right=" << right << " radius=" << radius << " pivot=" << pivot << " wordSeq.size()=" << wordSequence.size() << " in buildCoOccurrenceMatrix()" << endl;
      }

      //increment each co-occurrence
      while(left < right){
        if(wordSequence[left] != wordSequence[pivot]){ //check verifies we're not correlating a word with itself
          updateCoOccurrenceMatrix(outer->first,wordSequence[left],matrix); //this function ensures key symmetry, so no dupes in table
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
  for(i = 0, sim_outer = matrix.begin(); (i < 300) && sim_outer != matrix.end(); ++sim_outer){
    for(sim_inner = sim_outer->second.begin(); sim_inner != sim_outer->second.end(); ++sim_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      i++;
      cout << sim_outer->first << " " << sim_inner->first << "|" << sim_inner->second << endl;
    }
  }
*/



/*
  cout << "trimming dupes from table, init size=" << matrix.size() << endl;
  //previous algorthm using update() function should have prevented dupes, so this is not needed
  for(i = 0, sim_outer = matrix.begin(); sim_outer != matrix.end(); ++sim_outer){
    for(sim_inner = sim_outer->second.begin(); sim_inner != sim_outer->second.end(); ++sim_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      dupe = CoOccMatrix[sim_inner->first].find(sim_outer->first);
      if(dupe != CoOccMatrix[sim_inner->first].end()){
        CoOccMatrix[sim_inner->first].erase(dupe);
        i++;
      }
    }
  }
  cout << "removed >" << i << "< dupes. table size is now=" << matrix.size() << endl;
*/
/*
  struct timespec begin, end;
  clock_gettime(CLOCK_MONOTONIC,&begin);
  buildSimilarityMatrix(CoOccMatrix);
  clock_gettime(CLOCK_MONOTONIC,&end);
  end = DiffTimeSpecs(&begin,&end);
  cout << "\nsim table build() time: " << end.tv_sec << "s " << end.tv_nsec << "ns" << endl;
*/
/*
  //final step, correlate the words in the table using ??? Bordag equation (5)
  //however this is done, the important thing is just that we precompute these values
  for(sim_outer = matrix.begin(); sim_outer != matrix.end(); ++sim_outer){
    for(sim_inner = sim_outer->second.begin(); sim_inner != sim_outer->second.end(); ++sim_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      sim_inner->second = buildSimilarityTable(sim_outer->first, sim_inner->first);
    }
    cout << "\rcalc, outer i=" << (i++) << " of " << matrix.size() << "                       " << flush;
  }
*/


/*
  i = 0;
  //final step, correlate the words in the table using ??? Bordag equation (5)
  //however this is done, the important thing is just that we precompute these values
  for(sim_outer = matrix.begin(); sim_outer != matrix.end(); ++sim_outer){
    for(sim_inner = sim_outer->second.begin(); sim_inner != sim_outer->second.end(); ++sim_inner){
      //inner->second = correlateWords(outer->first, inner->first);
      sim_inner->second = calculateSignificance(sim_outer->first, sim_inner->first);
    }
    cout << "\rcalc, outer i=" << (i++) << " of " << matrix.size() << "                       " << flush;
  }
  cout << endl;
*/

  //matrixToFile(dbfile);

  cout << "DONE >> matrix.size(): " << matrix.size() << endl;
}



/*
  Converts a co-occurrence table into a similarity table, by calculating the cosine-similarity
  between every row and column.

  Given a table containing co-occurrence data (as counts), this iteratively replaces
  all the co-occurrence data with cosine-similarity values. This is essentially a matrix
  multiplication op, except that multiplication (dot products) is replaced with the cos-sim
  function. Note that to protect from writing-to data that will later be read-in and used
  in further calculation, we copy each row into an independent data structure, run the calculations,
  and then copy the row back into the actual similarity table.

  CoOccurrence matrix contains the event (word) cooccurrence data; similarity matrix will be written to.
  Since similarity is symmetric, similarity matrix can be 1/2 the size of the coccurrence matrix. I used
  to use a single matrix as the co-occurrence matrix, then converted it to a similarity matrix, but it was
  very bug prone and difficult to program. Using two matrices increases memory consumption, however portions
  of the co-occurrence matrix can be deleted after they are processed, but again it requires a careful strategy.
  NOTE co-occurrence matrix is deleted after this function executes, to minimize memory consumption. Each row
  in the co-occurrence row is deleted after it goes out of scope for any further calculation.
*/
void PersonalClassifier::buildSimilarityMatrix(Matrix& coOccurrenceMatrix, Matrix& similarityMatrix)
{
  double outerprogress, inputSize;
  FloatType angle;
  MatrixOuterIt sim_outer1, sim_outer2;
  MatrixInnerIt inner;
  //RowMap row;

  if(!similarityMatrix.empty()){
    cout << "WARN non-empty similarity matrix passd to buildSimilarityMatrix. Clearing previous contents." << endl;
    similarityMatrix.clear();
  }

  inputSize = (double)coOccurrenceMatrix.size();
  outerprogress = 0.0;
  //for each word, relate it with every other word (including itself, currently)
  for(sim_outer1 = coOccurrenceMatrix.begin(); sim_outer1 != coOccurrenceMatrix.end(); ++sim_outer1){
    //cout << "outer1=" << sim_outer1->first;

    /*
    //copy the row into a read-only temp map, since the row is being written-to and used for calculations simultaneously in the next loop
    if(!row.empty()){
      row.clear();
    }
    for(inner = sim_outer1->second.begin(); inner != sim_outer1->second.end(); ++inner){
      row[inner->first] = inner->second;
    }
    */

    //delete the read-from map
    //sim_outer1->second.clear();
    //cout << "sizes: " << row.size() << " " << sim_outer1->second.size() << endl;

    sim_outer2 = sim_outer1;
    for(++sim_outer2; sim_outer2 != coOccurrenceMatrix.end(); ++sim_outer2){
      //HACK a space heuristic, to only compare words that share at least one co-occurrence
      if(shareCoOccurrences(sim_outer1,sim_outer2)){
      //if(shareCoOccurrences(coOccurrenceMatrix,sim_outer1->first,sim_outer2->first)){ //only evaluate words sharing at least one co-occurrence
        //angle = method_binaryCosine(sim_outer1,sim_outer2);
        angle = method_weightedCosine(sim_outer1->second,sim_outer2->second);
        if(isnormal(angle)){  //prevents nans
          //only one key relation needs to be stored, since cosine similarity is symmetric (see header)
          //note the pattern for this will create a triangular/diagonal matrix. Many trees for the first keys, tapering to near 1 at end keys.
          similarityMatrix[sim_outer1->first][sim_outer2->first] = angle;

          //dbg
          if(angle > 1.0){
            cout << "ERROR Math logic error in method_weightCosine. Ret cannot be greater than 1.0, by def cosine-sim.\n Word/val pair is: ";
            cout << m_wordIdManager->idToString(sim_outer1->first) << "/" << m_wordIdManager->idToString(sim_outer2->first) << "/" << angle << endl;
          }
        }
        else{
          if(angle != 0.0){
            cout << "WARN abnormal value >" << angle << "< for " << sim_outer1->first << "\\" << sim_outer2->first << endl;
          }
          //else: don't waste space on zero entries, just return 0.0 for non-hits in the look up methods
          //similarityMatrix[sim_outer1->first][sim_outer2->first] = 0.0;
        }
        //cout << "similarity " << wordIdTable[sim_outer1->first] << "|" << wordIdTable[sim_outer2->first]<< ": " << inner->second << endl;
        /*
        if(inner->second > 0.95){
          cout << "similarity " << wordIdTable[sim_outer1->first] << "|" << wordIdTable[sim_outer2->first]<< ": " << inner->second << endl;
        }
        */
      }

      //progress++;
      //cout << "inner progress: " << (100 * (progress / (double)sim_outer1->second.size())) << "% completed             \r" << flush;
    }
    //clear previous row's map
    sim_outer1->second.clear();
    outerprogress++;
    cout << "\router progress: " << (100 * (outerprogress / inputSize)) << "% completed " << outerprogress << "/" << inputSize << "     " << flush;
  }

  cout << "buildSimilarityMatrix() complete. Co-Occurrence matrix being deleted..." << endl;
  coOccurrenceMatrix.clear();
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

  Doesn't initialize U32 word ids, since on start there may be > 65535 unique words. Only write the ids
  after pruning the word set.
*/
U32 PersonalClassifier::buildWordIndexTable(const vector<string>& wordSequence, WordIndexTable& wordIndexTable)
{
  U32 max, i;
  vector<string>::iterator it;
  WordIndexTableIt maxit;
  WordIndexTableIt mapIt;

  if(!wordIndexTable.empty()){
    cout << "WARN wordIndexTable not empty in buildWordIndexTable(). Clearing previous table contents..." << endl;
    wordIndexTable.clear();
  }

  //builds the sequence dictionary:  word -> occurrences[1,34,76,300...]
  for(i = 0; i < wordSequence.size(); i++){
    wordIndexTable[ wordSequence[i] ].push_back(i);
  }

  //dbg: find the max. this isn't necessary except for tracking purposes
  max = 0; i = 0;
  for(mapIt = wordIndexTable.begin(); mapIt != wordIndexTable.end(); ++mapIt){
    if(mapIt->second.size() > max){
      maxit = mapIt;
      max = mapIt->second.size();
    }
  }
  cout << "max word.ct " << max << " for: " << maxit->first << " in buildWordIndexTable" << endl;

  return max;
}

//a terribly nested, single context function
/*
  Checks whether or not two U32 keys entries share any co-occurrences. Checks for [key1][key2] or [key2][key1] entries.
*/
bool PersonalClassifier::shareCoOccurrences(MatrixOuterIt& row1, MatrixOuterIt& row2)
{
  bool ret = false;

  //if there is any entry, we assume its non-zero, and thus these two entries share a co-occurrence
  if(row2->second.find( row1->first ) != row2->second.end()){
    ret = true;
  }
  //assume the else isn't necessary. This is only the case if we can assume keys are stored symmetrically,
  //that is, the co-occurrence matrix stores both [key1][key2] and [key2][key1] (even though they store the same count)
  //This func is called at high-frequency, so this assumption may be very good for performance.
  /*
  else if(row1->second.find( row2->first ) != row1->second.end()){
    ret = true;
  }
  */

  return ret;
}

//a terribly nested, single context function
/*
  Checks whether or not two U32 keys share any co-occurrences. Checks for [key1][key2] or [key2][key1] entries.

bool PersonalClassifier::shareCoOccurrences(Matrix& matrix, U32 key1, U32 key2)
{
  MatrixOuterIt outer;
  MatrixInnerIt inner;

  if(matrix.empty()){ //logic exception
    cout << "ERROR logic error in shareCoOccurrences()" << endl;
    return false;
  }

  //look for [key1][key2] in matrix instead of row
  outer = matrix.find(key1);
  if(outer != matrix.end()){
    inner = outer->second.find(key2);
    if(inner != outer->second.end()){
      return true;
    }
  }

  //look for [key2][key1]
  outer = matrix.find(key2);
  if(outer != matrix.end()){
    inner = outer->second.find(key1);
    if(inner != outer->second.end()){
      return true;
    }
  }

  return false;
}
*/



/*check if the CoOccurrence table already has some key, which
  is defined as (word1,word2) in either order. Since word1+word2
  and word2+word1 are technically unique keys, this function
  allows us to conflate them as ne logical key. This halves the potential
  size of our table by eliminating duplication, as the following is true:
    dist(word1,word2) == dist(word2,word1)
  Therefore, CoOcc[word1][word2] should always equal CoOcc[word2][word1], and we
  can eliminate at least one of these entries.
  
  Returns false if neither dict[k1][k2] nor dict[k2][k1] exists in table
  If true, iterator of location is stored in _it.

*/
bool PersonalClassifier::matrixHasKey(Matrix& matrix, const U32 key1, const U32 key2, MatrixInnerIt& it)
{
  MatrixOuterIt outer;

  if(matrix.empty()){ //logic exception
    return false;
  }

  outer = matrix.find(key1);
  if(outer != matrix.end()){
    it = outer->second.find(key2);
    if(it != outer->second.end()){
      return true;
    }
  }
  outer = matrix.find(key2);
  if(outer != matrix.end()){
    it = outer->second.find(key1);
    if(it != outer->second.end()){
      return true;
    }
  }

  return false;
}

bool PersonalClassifier::matrixHasKey(Matrix& matrix, const string& k1, const string& k2, MatrixInnerIt& it)
{
  U32 key1 = m_wordIdManager->stringToId(k1);
  U32 key2 = m_wordIdManager->stringToId(k2);

  if(key1 == 0){
    cout << "ERROR key1=0 for k1=" << k1 << " in matrixHasKey" << endl;  
    return false;
  }
  if(key2 == 0){
    cout << "ERROR key2=0 for k2=" << k2 << " in matrixHasKey" << endl;  
    return false;
  }
  if(matrix.empty()){ //logic exception
    return false;
  }

  return matrixHasKey(matrix,key1,key2,it);

/*
  if(matrix.find(key1) != matrix.end()){
    if(CoOccMatrix[key1].find(key2) != CoOccMatrix[key1].end()){
      return true;
    }
  }
  if(matrix.find(key2) != matrix.end()){
    if(CoOccMatrix[key2].find(key1) != CoOccMatrix[key2].end()){
      return true;
    }
  }
  return false;
*/
}


//compares two words as co-occurrence vectors
FloatType PersonalClassifier::method_binaryCosine(RowMap& row1, RowMap& row2)
{
  //bool hit1, hit2;  //flags for if some word has a co-occurrence (any non-zero value) for some other word
  FloatType n12, n1, n2;
  MatrixInnerIt inner;

  n1 = row1.size();
  n2 = row2.size();

  n12 = 0.0;
  //iterate in parallel over both co-occurrence vectors
  for(U32 i = 0; i < m_wordIdManager->getIdCounter(); i++){

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
/*
//compares two words as co-occurrence vectors. Faster version. 
FloatType PersonalClassifier::method_weightedCosine(const RowMap& row1, const RowMap& row2)
{
  FloatType n12, ret, n1, n2;
  ConstMatrixInnerIt inner1, inner2;

  ret = n12 = n1 = n2 = 0.0;
  
  for(inner1 = row1.cbegin(); inner1 != row1.end(); ++inner1){
    n1 += pow(inner1->second,2.0);

    inner2 = row2.find(inner1->second);
    if(inner2 != row2.end()){
      n12 += (inner1->second * inner2->second);
    }
  }


  if(row1.size() < row2.size()){
    //row1 is smaller, so iterate that set
    for(inner1 = row1.cbegin(); inner1 != row1.end(); ++inner1){
      n1 += pow(inner1->second,2.0);

      inner2 = row2.find(inner1->second);
      if(inner2 != row2.end()){
        n12 += (inner1->second * inner2->second);
      }
    }
    //post-loop: n1 and n12 counts set

    //...and now get n2 count
    for(inner2 = row2.cbegin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);
    }
  }
  else{
    //row2 is the smaller set, so use it to perform lookups in the other set
    for(inner2 = row2.cbegin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);

      inner1 = row1.find(inner2->second);
      if(inner1 != row1.end()){
        n12 += (inner2->second * inner1->second);
      }
    }
    //post-loop: n2 and n12 counts set
    
    //...and now get n1 count
    for(inner1 = row1.cbegin(); inner1 != row1.end(); ++inner1){
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
*/


//compares two words as co-occurrence vectors. Faster version. 
FloatType PersonalClassifier::method_weightedCosine(const RowMap& row1, const RowMap& row2)
{
  FloatType n12, ret, n1, n2;
  ConstMatrixInnerIt inner1, inner2;

  ret = n12 = n1 = n2 = 0.0;
  
  //iterate SMALLER row, correlating with larger set. This speeds things up by looking up only the largest possible size of the set intersection (the size of the smaller set)
  //This will normally be very effective, since most set sizes will be somewhat different.
  if(row1.size() < row2.size()){
    //row1 is smaller, so iterate that set
    for(inner1 = row1.cbegin(); inner1 != row1.end(); ++inner1){
      n1 += pow(inner1->second,2.0);

      inner2 = row2.find(inner1->first);
      if(inner2 != row2.end()){
        n12 += (inner1->second * inner2->second);
      }
    }
    //post-loop: n1 and n12 counts set

    //...and now get n2 count
    for(inner2 = row2.cbegin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);
    }
  }
  else{
    //row2 is the smaller set, so use it to perform lookups in the other set
    for(inner2 = row2.cbegin(); inner2 != row2.end(); ++inner2){
      n2 += pow(inner2->second,2.0);

      inner1 = row1.find(inner2->first);
      if(inner1 != row1.end()){
        n12 += (inner2->second * inner1->second);
      }
    }
    //post-loop: n2 and n12 counts set
    
    //...and now get n1 count
    for(inner1 = row1.cbegin(); inner1 != row1.end(); ++inner1){
      n1 += pow(inner1->second,2.0);
    }
  }
  //post: have counts of n1, n2, and n12

  //zero check to prevent nans
  if((n12 > 0.0) && (n1 > 0.0) && (n2 > 0.0)){
    //assign binary cosine val to w1 x w2
    ret =  (FloatType)(n12 / (sqrt(n1) * sqrt(n2)));
  }

  //returns 0.0 for words which shared no second-order co-occurrences
  return ret;
}

/*
  Just another exception-case/single-context function. Builds a word index table based off
  Matrix keys. This means the Matrix is in memory, but for some reason
  the wordIndexTable isn't (such as if we rebuilt from a file).
  Side-effects may include that the wordIndexTable therefore only contains the set of items
  that are in the Matrix, but this is a condition we try to satisfy anyway. Also
  you won't have the word indices for a given word, every word will just have a single item
  vector containing integer-index 0.

void PersonalClassifier::buildDummyWordIndexTable(void)
{
  string s;
  MatrixOuterIt outer;
  MatrixInnerIt inner;

  for(outer = matrix.begin(); outer != matrix.end(); ++outer){
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
*/




