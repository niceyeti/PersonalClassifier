#include "PersonalClassifier.hpp"

/*
  Currently all n-gram str::ct are held in memory, calculated, then dumped to file.
  For very large input, we may instead need to separate parsing, counting, and calculating
  by outputting to file at each stage. Other indexing methods may be used as well.
*/
int main(int argc, char* argv[])
{
  int i, priority = -19;
  id_t pid;
  char buf[256];
  string testString = "your";
  string seed;
  string testFile = "../testText.txt";
  string inputFile = "../trainingText.txt";
  string parsedFile = "../parsedCorpus.txt";
  string enronCorpus = "../../nGram/enronParsed/parsed.txt";
  PersonalClassifier personalClassifier("../stopWords1.txt");
  std::multimap<string,int>  myMap1;
  std::multimap< string, std::multimap<string,int> > myMap2;

  srand(time(NULL));
  pid = getpid();
  cout << "Some system stats..." << endl;
  cout << "sizeof: ulong long=" << sizeof(unsigned long long int) << "  long uint=" << sizeof(U64) << "  long double=" << sizeof(long double) << "   double=" << sizeof(double) << endl;
  cout << "procId[" << (int)pid << "] priority: " << (int)getpriority(PRIO_PROCESS, pid) << endl;
  setpriority(PRIO_PROCESS, pid, priority);
  cout << "procId[" << (int)pid << "] priority: " << (int)getpriority(PRIO_PROCESS, pid) << " after call to setpriority(-19). Exec as sudo if priority did not change or is not -19." << endl;
  cout << "Jesse: set proc priority high, set memsize large if possible" << endl;
  cout << "sizeof(multimap<string>)= " << sizeof(myMap1) << "  sizeof(multimap<multimap>) = " << sizeof(myMap2) << endl;
  if(sizeof(U32) != 4){
    cout << "ERROR date encoding requires 4-byte U32 size, exiting." << endl;
    return 0;
  }

  string personalTraining = "../../nGram/enronParsed/parsed.txt";
  string personalSample = "../../nGram/enronParsed/phony.txt";
  personalClassifier.testPerson(personalTraining,personalSample);


  return 0;
}

/*
  unordered_map<string,vector<string> > clusters;
  vector<string> means;

  string s = "hello";
  means.push_back(s);
  s = "nil";
  means.push_back(s);
  s = "dolly";
  means.push_back(s);
  seed = "blah";
  clusters[means[0]].push_back(seed);
  clusters[means[0]].push_back(seed);
  clusters[means[0]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);
  clusters[means[1]].push_back(seed);

  nnModel.writeClusters(clusters,means);
  cout << "done" << endl;
  exit(0);

*/

