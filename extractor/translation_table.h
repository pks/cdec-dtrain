#ifndef _TRANSLATION_TABLE_
#define _TRANSLATION_TABLE_

#include <memory>
#include <string>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>

using namespace std;
namespace fs = boost::filesystem;

class Alignment;
class DataArray;

typedef boost::hash<pair<int, int> > PairHash;

class TranslationTable {
 public:
  TranslationTable(
      shared_ptr<DataArray> source_data_array,
      shared_ptr<DataArray> target_data_array,
      shared_ptr<Alignment> alignment);

  virtual ~TranslationTable();

  virtual double GetTargetGivenSourceScore(const string& source_word,
                                           const string& target_word);

  virtual double GetSourceGivenTargetScore(const string& source_word,
                                           const string& target_word);

  void WriteBinary(const fs::path& filepath) const;

 protected:
  TranslationTable();

 private:
  void IncreaseLinksCount(
      unordered_map<int, int>& source_links_count,
      unordered_map<int, int>& target_links_count,
      unordered_map<pair<int, int>, int, PairHash>& links_count,
      int source_word_id,
      int target_word_id) const;

  shared_ptr<DataArray> source_data_array;
  shared_ptr<DataArray> target_data_array;
  unordered_map<pair<int, int>, pair<double, double>, PairHash>
      translation_probabilities;
};

#endif
