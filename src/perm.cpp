#include <iostream>
#include <vector>
#include <fstream>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <chrono>

using namespace std;

constexpr size_t Dimensions = 768;

array<uint16_t, Dimensions> greedy(array<vector<uint64_t>, Dimensions>& log) {
  size_t count[Dimensions] = { 0 };
  for (size_t i = 0; i < Dimensions; ++i)
  {
    for (auto x: log[i])
      count[i] += __builtin_popcountll(x);
  }

  vector<uint16_t> sorted_by_count;
  for (size_t i = 0; i < Dimensions; ++i)
    sorted_by_count.push_back(i);
  std::sort(sorted_by_count.begin(), sorted_by_count.end(), [&](auto a, auto b){ return count[a] < count[b]; });

  array<uint16_t, Dimensions> out;
  vector<uint64_t> combined;
  combined.resize(log[0].size());
  for (size_t chunk = 0; chunk < Dimensions / 4; ++chunk)
  {
    for (size_t i = 0; i < log[0].size(); ++i)
    {
      combined[i] = log[sorted_by_count[0]][i];
    }
    out[chunk*4] = sorted_by_count[0];
    sorted_by_count.erase(sorted_by_count.begin());
    for (size_t k = 1; k < 4; ++k)
    {
      size_t min = -1;
      uint16_t min_index = 0;
      
      for (size_t l = 0; l < sorted_by_count.size(); ++l)
      {
        if (min <= count[sorted_by_count[l]])
          break;
        size_t current = 0;
        for (size_t i = 0; i < log[0].size(); ++i)
        {
          current += __builtin_popcountll(log[sorted_by_count[l]][i] | combined[i]);
        }
        if (current < min)
        {
          min = current;
          min_index = l;
        }
      }
      for (size_t i = 0; i < log[0].size(); ++i)
      {
        combined[i] |= log[sorted_by_count[min_index]][i];
      }
      out[chunk*4 + k] = sorted_by_count[min_index];
      sorted_by_count.erase(sorted_by_count.begin()+min_index);
    }
    std::cout << "Chunk " << chunk + 1 << "/" << Dimensions/4 << endl;
  }
  return out;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
  {
    cout << "Usage: perm <file>" << endl
         << "<file> is a binary file containing arrays of uint_64 of length Dimensions." << endl
         << "Each nonzero bit in array[i] represents a nonzero activation" << endl
         << "of ith neuron. The nth bit of all elements of the array" << endl
         << "corresponds to the same position." << endl;
    return 0;
  }
  array<vector<uint64_t>, Dimensions> log;
  fstream file;
  file.open(argv[1], ios::in | ios::binary);
  if (!file.is_open())
  {
    cout << "Couldn't open file " << argv[1] << endl;
    return 0;
  }
  size_t len;
  file.read(reinterpret_cast<char*>(&len), 8);
  for (size_t i = 0; i < len ; ++i) {
    if (!file)
      break;
    array<uint64_t, Dimensions> buffer;
    file.read(reinterpret_cast<char*>(&buffer[0]), Dimensions * 8);
    for (size_t i = 0; i < Dimensions; ++i)
    {
      log[i].push_back(buffer[i]);
    }
  }
  std::cerr << "Loaded log file with " << log[0].size()*64 << " positions" << endl;
  auto begin = std::chrono::high_resolution_clock::now();
  auto perm = greedy(log);
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Calculated permutation in " << std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count() << "ms" << std::endl;
  for (auto x: perm)
    std::cout << x << " ";
  std::cout << endl;
}
