#ifndef _WORDLE_H_
#define _WORDLE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class Wordle {
 public:
  Wordle() {}

  Wordle(size_t wordSize, const std::string &pathToWords) : wordSize(wordSize) {
    LoadDictionary(pathToWords);
  }

  Wordle(const Wordle &other) = delete;
  Wordle(Wordle &&other) = delete;

  ~Wordle() {}

  std::optional<std::string> PlayRandomGame();
  void PlayOffLineGame();

 private:
  size_t wordSize;
  std::vector<std::string> dictionary;

  void LoadDictionary(const std::string &path);
  std::string GetNextBestGuess(const std::vector<std::string> &validWords);
};

#endif
