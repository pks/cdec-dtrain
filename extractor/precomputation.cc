#include "precomputation.h"

#include <iostream>
#include <queue>

#include "data_array.h"
#include "suffix_array.h"

using namespace std;
using namespace tr1;

int Precomputation::NON_TERMINAL = -1;

Precomputation::Precomputation(
    shared_ptr<SuffixArray> suffix_array, int num_frequent_patterns,
    int num_super_frequent_patterns, int max_rule_span,
    int max_rule_symbols, int min_gap_size,
    int max_frequent_phrase_len, int min_frequency) {
  vector<int> data = suffix_array->GetData()->GetData();
  vector<vector<int> > frequent_patterns = FindMostFrequentPatterns(
      suffix_array, data, num_frequent_patterns, max_frequent_phrase_len,
      min_frequency);

  unordered_set<vector<int>, VectorHash> frequent_patterns_set;
  unordered_set<vector<int>, VectorHash> super_frequent_patterns_set;
  for (size_t i = 0; i < frequent_patterns.size(); ++i) {
    frequent_patterns_set.insert(frequent_patterns[i]);
    if (i < num_super_frequent_patterns) {
      super_frequent_patterns_set.insert(frequent_patterns[i]);
    }
  }

  vector<tuple<int, int, int> > matchings;
  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] == DataArray::END_OF_LINE) {
      AddCollocations(matchings, data, max_rule_span, min_gap_size,
          max_rule_symbols);
      matchings.clear();
      continue;
    }
    vector<int> pattern;
    for (int j = 1; j <= max_frequent_phrase_len && i + j <= data.size(); ++j) {
      pattern.push_back(data[i + j - 1]);
      if (frequent_patterns_set.count(pattern)) {
        inverted_index[pattern].push_back(i);
        int is_super_frequent = super_frequent_patterns_set.count(pattern);
        matchings.push_back(make_tuple(i, j, is_super_frequent));
      } else {
        // If the current pattern is not frequent, any longer pattern having the
        // current pattern as prefix will not be frequent.
        break;
      }
    }
  }
}

Precomputation::Precomputation() {}

Precomputation::~Precomputation() {}

vector<vector<int> > Precomputation::FindMostFrequentPatterns(
    shared_ptr<SuffixArray> suffix_array, const vector<int>& data,
    int num_frequent_patterns, int max_frequent_phrase_len, int min_frequency) {
  vector<int> lcp = suffix_array->BuildLCPArray();
  vector<int> run_start(max_frequent_phrase_len);

  priority_queue<pair<int, pair<int, int> > > heap;
  for (size_t i = 1; i < lcp.size(); ++i) {
    for (int len = lcp[i]; len < max_frequent_phrase_len; ++len) {
      int frequency = i - run_start[len];
      // TODO(pauldb): Only add patterns that don't span across multiple
      // sentences.
      if (frequency >= min_frequency) {
        heap.push(make_pair(frequency,
            make_pair(suffix_array->GetSuffix(run_start[len]), len + 1)));
      }
      run_start[len] = i;
    }
  }

  vector<vector<int> > frequent_patterns;
  for (size_t i = 0; i < num_frequent_patterns && !heap.empty(); ++i) {
    int start = heap.top().second.first;
    int len = heap.top().second.second;
    heap.pop();

    vector<int> pattern(data.begin() + start, data.begin() + start + len);
    frequent_patterns.push_back(pattern);
  }
  return frequent_patterns;
}

void Precomputation::AddCollocations(
    const vector<tuple<int, int, int> >& matchings, const vector<int>& data,
    int max_rule_span, int min_gap_size, int max_rule_symbols) {
  for (size_t i = 0; i < matchings.size(); ++i) {
    int start1, size1, is_super1;
    tie(start1, size1, is_super1) = matchings[i];

    for (size_t j = i + 1; j < matchings.size(); ++j) {
      int start2, size2, is_super2;
      tie(start2, size2, is_super2) = matchings[j];
      if (start2 - start1 >= max_rule_span) {
        break;
      }

      if (start2 - start1 - size1 >= min_gap_size
          && start2 + size2 - start1 <= max_rule_span
          && size1 + size2 + 1 <= max_rule_symbols) {
        vector<int> pattern(data.begin() + start1,
            data.begin() + start1 + size1);
        pattern.push_back(Precomputation::NON_TERMINAL);
        pattern.insert(pattern.end(), data.begin() + start2,
            data.begin() + start2 + size2);
        AddStartPositions(collocations[pattern], start1, start2);

        if (is_super2) {
          pattern.push_back(Precomputation::NON_TERMINAL);
          for (size_t k = j + 1; k < matchings.size(); ++k) {
            int start3, size3, is_super3;
            tie(start3, size3, is_super3) = matchings[k];
            if (start3 - start1 >= max_rule_span) {
              break;
            }

            if (start3 - start2 - size2 >= min_gap_size
                && start3 + size3 - start1 <= max_rule_span
                && size1 + size2 + size3 + 2 <= max_rule_symbols
                && (is_super1 || is_super3)) {
              pattern.insert(pattern.end(), data.begin() + start3,
                  data.begin() + start3 + size3);
              AddStartPositions(collocations[pattern], start1, start2, start3);
              pattern.erase(pattern.end() - size3);
            }
          }
        }
      }
    }
  }
}

void Precomputation::AddStartPositions(
    vector<int>& positions, int pos1, int pos2) {
  positions.push_back(pos1);
  positions.push_back(pos2);
}

void Precomputation::AddStartPositions(
    vector<int>& positions, int pos1, int pos2, int pos3) {
  positions.push_back(pos1);
  positions.push_back(pos2);
  positions.push_back(pos3);
}

void Precomputation::WriteBinary(const fs::path& filepath) const {
  FILE* file = fopen(filepath.string().c_str(), "w");

  // TODO(pauldb): Refactor this code.
  int size = inverted_index.size();
  fwrite(&size, sizeof(int), 1, file);
  for (auto entry: inverted_index) {
    size = entry.first.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(entry.first.data(), sizeof(int), size, file);

    size = entry.second.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(entry.second.data(), sizeof(int), size, file);
  }

  size = collocations.size();
  fwrite(&size, sizeof(int), 1, file);
  for (auto entry: collocations) {
    size = entry.first.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(entry.first.data(), sizeof(int), size, file);

    size = entry.second.size();
    fwrite(&size, sizeof(int), 1, file);
    fwrite(entry.second.data(), sizeof(int), size, file);
  }
}

const Index& Precomputation::GetInvertedIndex() const {
  return inverted_index;
}

const Index& Precomputation::GetCollocations() const {
  return collocations;
}