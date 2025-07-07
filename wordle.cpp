// wordle.cpp
// Implementation file for the command line Wordle solver.  Contains
// utility functions for pattern generation and entropy based guessing
// as well as the entry point used for offline play or testing.
#include "wordle.h"  // Wordle class header

#include <algorithm>      // For algorithms like copy_if, find
#include <array>          // For std::array
#include <cassert>        // For assert
#include <fstream>        // For file I/O
#include <iostream>       // For console I/O
#include <map>            // For ordered map (used in entropy calculation)
#include <random>         // For random number generation
#include <string>         // For std::string
#include <unordered_map>  // For frequency counts and pattern counts
#include <unordered_set>  // For storing unique words

using namespace std;

// Alias for a map keyed by entropy value (double) with a vector of words,
// using a custom comparator for doubles.
template <typename Comp>
using EntropiesMap = map<double, vector<string>, Comp>;

// Custom comparator for double values, using an epsilon to account for
// floating-point precision.
struct CompareDouble {
  bool operator()(double a, double b) const {
    constexpr double EPSILON = 1e-9;  // Precision threshold
    return (a - b) < -EPSILON;
  }
};

// Array of file names from the WordNet dictionary.
static constexpr array<const char *, 4> wordnetFiles = {
    "dict/index.noun", "dict/index.verb", "dict/index.adj", "dict/index.adv"};

//// Helper Functions for Pattern Manipulation
// A Pattern is stored as an int where each letter's feedback is encoded in 2
// bits. Colors are defined as:
using Pattern = int;
using Color = enum { Gray = 0, Yellow = 1, Green = 2 };

// SetColor: Sets the color for a given position (0-indexed) in the pattern.
// The pattern uses 2 bits per letter, so position 'index' uses bits [2*index,
// 2*index+1].
static void SetColor(Pattern &p, int index, Color c) {
  if (index >= 0 && index <= 7) {
    int shift = index * 2;
    // Clear the 2 bits at the given position and set them to c.
    p = (p & (~(3 << shift))) | (c << shift);
  } else {
    throw std::invalid_argument("Invalid index in SetColor");
  }
}

// GetColor: Retrieves the color for a given position in the pattern.
static Color GetColor(Pattern &p, int index) {
  if (index >= 0 && index <= 7) {
    int shift = 2 * index;
    // Extract the 2 bits and cast to Color.
    return static_cast<Color>(((p & (3 << shift)) >> shift) & 3);
  }
  throw std::invalid_argument("Invalid index in GetColor");
}

// GeneratePattern: Generates a feedback pattern for a given guess against the
// actual word. 'wordSize' indicates the number of letters in the word.
static Pattern GeneratePattern(const string &guess, const string &other,
                               const size_t wordSize) {
  Pattern p = 0;  // Initialize pattern to 0 (default Gray)
  unordered_map<char, int>
      freq;  // Frequency map of characters in the actual word

  // Count the frequency of each letter in the actual word.
  for (auto &c : other) {
    freq[c]++;
  }

  // First pass: Mark Green (correct letter in the correct position).
  // Note: We fill from rightmost position (wordSize - 1 - i) for consistency.
  for (int i = 0; i < guess.size(); ++i) {
    if (guess[i] == other[i]) {
      freq[guess[i]]--;  // Consume one occurrence of the letter.
      SetColor(p, wordSize - 1 - i, Green);
    }
  }

  // Second pass: For letters not marked Green, mark Yellow if the letter
  // exists, or Gray otherwise.
  for (int i = 0; i < guess.size(); ++i) {
    char c = guess[i];
    if (GetColor(p, wordSize - 1 - i) == Green) {
      continue;  // Skip letters already marked Green.
    }
    if (freq[c]) {
      freq[c]--;  // Consume an occurrence for Yellow.
      SetColor(p, wordSize - 1 - i, Yellow);
    } else {
      SetColor(p, wordSize - 1 - i, Gray);
    }
  }

  return p;
}

// extractWords: Reads words from a given file and inserts them into 'wordDict'.
// Each word is assumed to be separated by a space.
static bool extractWords(const string &fileName,
                         unordered_set<string> &wordDict) {
  ifstream file(fileName);
  if (!file.is_open()) {
    cerr << "Error: Could not open " << fileName << endl;
    return false;
  }

  string line;
  while (getline(file, line)) {
    if (line.empty() || line[0] == ' ') continue;
    size_t pos = line.find(' ');
    if (pos != string::npos) {
      string word = line.substr(0, pos);
      wordDict.insert(word);
    }
  }
  file.close();
  return true;
}

// GetRandomGuess: Returns a random word from the validWords vector.
static auto GetRandomGuess(const vector<string> &validWords) -> string {
  mt19937 mt(random_device{}());
  uniform_int_distribution<int> dist(0, validWords.size() - 1);
  return validWords[dist(mt)];
}

// CalculateEntropy: Computes the Shannon entropy for a distribution of
// patterns. 'counts' maps a Pattern to the number of occurrences.
static double CalculateEntropy(const unordered_map<Pattern, int> &counts) {
  int sumCounts = 0;
  for (auto &p : counts) {
    sumCounts += p.second;
  }
  double entropy = 0.0;
  for (auto &c : counts) {
    double p = static_cast<double>(c.second) / sumCounts;
    if (p > 0.0) {
      entropy += p * log2(p);
    }
  }
  return -entropy;
}

// CalculateEntropies: For each word in 'words', simulate comparisons against
// all other words to compute its entropy. Results are stored in 'entropies'
// (map keyed by entropy value).
static void CalculateEntropies(const vector<string> &words,
                               const size_t wordSize,
                               EntropiesMap<CompareDouble> &entropies) {
  const size_t N = words.size();
  for (size_t i = 0; i < N; i++) {
    unordered_map<Pattern, int> pCounters;
    for (int j = 0; j < N; ++j) {
      if (i != j) {
        pCounters[GeneratePattern(words[i], words[j], wordSize)]++;
      }
    }
    entropies[CalculateEntropy(pCounters)].push_back(words[i]);
  }
}

// FilterAllWordsBestOnPattern: Filters validWords to only those words that
// produce the pattern 'p' when compared with the given guess.
auto FilterAllWordsBestOnPattern(const Pattern &p, const string &guess,
                                 const vector<string> &validWords,
                                 const size_t wordSize) -> vector<string> {
  vector<string> result;
  copy_if(begin(validWords), end(validWords), back_inserter(result),
          [&p, &guess, wordSize](const string &w) {
            return GeneratePattern(guess, w, wordSize) == p;
          });
  return result;
}

// Wordle::LoadDictionary: Loads words from the WordNet files (using
// 'extractWords') and stores only words of the correct length (wordSize) into
// the class dictionary.
void Wordle::LoadDictionary(const string &path) {
  unordered_set<string> tempDict;
  for (auto &file : wordnetFiles) {
    if (!extractWords(path + "//" + string(file), tempDict)) {
      cerr << "Failed to read file " << file << endl;
    }
  }
  for (auto &s : tempDict) {
    if (s.size() == wordSize) {
      dictionary.emplace_back(s);
    }
  }
}

// Wordle::GetNextBestGuess: Calculates the entropy for each valid word and
// returns the word with the highest entropy (i.e., the one expected to yield
// the most information).
auto Wordle::GetNextBestGuess(const vector<string> &validWords) -> string {
  EntropiesMap<CompareDouble> entropies;
  CalculateEntropies(validWords, wordSize, entropies);
  if (entropies.empty()) {
    assert(validWords.size() > 1);
    return validWords[0];
  }
  return prev(end(entropies))->second[0];
}

// Wordle::PlayRandomGame: Simulates playing a random Wordle game.
// A random word is chosen from the dictionary, and guesses are generated
// until either the word is guessed or the maximum allowed guesses are reached.
auto Wordle::PlayRandomGame() -> optional<string> {
  vector<string> validWords(dictionary.begin(), dictionary.end());
  string wordToGuess = GetRandomGuess(validWords);
  string guess;
  // Allow up to wordSize+1 guesses.
  for (int guesses = 0; guesses < wordSize + 1 && guess != wordToGuess;
       ++guesses) {
    // For each guess, generate the best guess from current validWords.
    guess = GetNextBestGuess(validWords);
    Pattern p = GeneratePattern(guess, wordToGuess, wordSize);
    Pattern allGreen = 0;
    // Build the all-green pattern (i.e., every letter is Green).
    for (int i = 0; i < wordSize; ++i) {
      allGreen |= (2 << (i * 2));
    }
    if (p != allGreen) {
      // Filter validWords based on the feedback pattern.
      validWords = FilterAllWordsBestOnPattern(p, guess, validWords, wordSize);
    } else {
      return optional<string>(guess);
    }
  }
  return nullopt;
}

// Wordle::PlayOffLineGame: Interactive offline mode where the user can enter
// commands to play the game. The user can provide a word, request guesses,
// remove words from the dictionary, and input feedback patterns.
void Wordle::PlayOffLineGame() {
  cout << "Play Wordle Offline\n"
       << "Commands:\n"
       << "  word    - enter your word\n"
       << "  guess   - get the first/next guess\n"
       << "  remove  - remove the last guess from the dictionary\n"
       << "  quit    - exit the game\n";

  vector<string> validWords(dictionary.begin(), dictionary.end());
  string command, lastGuess;

  // Lambda: promptPatterns - prompts user for each letter's feedback pattern.
  auto promptPatterns = [this]() -> vector<string> {
    vector<string> pats;
    for (int i = 0; i < wordSize;) {
      cout << "Enter pattern for letter " << (i + 1)
           << " (gn for Green, y for Yellow, gr for Gray): ";
      string pat;
      getline(cin, pat);
      if (pat == "quit" || pat == "remove") break;
      if (pat != "gn" && pat != "y" && pat != "gr") {
        cout << "Invalid input!\n";
        continue;
      }
      pats.push_back(pat);
      i++;
    }
    return pats;
  };

  // Lambda: buildPattern - constructs a Pattern from user-entered feedback.
  auto buildPattern = [this](const vector<string> &pats) -> Pattern {
    Pattern p = 0;
    for (int i = 0; i < pats.size(); ++i) {
      Color c = (pats[i] == "gn") ? Green : (pats[i] == "y") ? Yellow : Gray;
      // Fill pattern from rightmost position (wordSize - 1 - i).
      SetColor(p, wordSize - 1 - i, c);
    }
    return p;
  };

  // Lambda: buildAllGreen - constructs the all-green pattern.
  auto buildAllGreen = [this]() -> Pattern {
    Pattern ag = 0;
    for (int i = 0; i < wordSize; ++i) ag |= (2 << (i * 2));
    return ag;
  };

  Pattern allGreen = buildAllGreen();

  // Main interactive loop.
  do {
    getline(cin, command);
    if (command == "guess") {
      cout << "Finding next guess...\n";
      lastGuess = GetNextBestGuess(validWords);
      cout << lastGuess << "\n";
    } else if (command == "word") {
      cout << "Enter your word: ";
      getline(cin, lastGuess);
      cout << "You can now start providing results.\n";
    } else if (command == "remove") {
      if (lastGuess.empty()) {
        cout << "No guess to remove.\n";
      } else {
        auto it = find(validWords.begin(), validWords.end(), lastGuess);
        if (it != validWords.end()) {
          cout << "Removing " << lastGuess << " from the dictionary.\n";
          validWords.erase(it);
        }
      }
    } else if (command == "quit") {
      cout << "Quitting game.\n";
      break;
    }

    // If a valid command was entered (other than "remove" or "quit") and
    // lastGuess is set:
    if (!command.empty() && !lastGuess.empty() && command != "remove" &&
        command != "quit") {
      vector<string> patterns = promptPatterns();
      if (patterns.empty()) continue;
      Pattern p = buildPattern(patterns);
      if (p != allGreen) {
        validWords =
            FilterAllWordsBestOnPattern(p, lastGuess, validWords, wordSize);
        lastGuess = GetNextBestGuess(validWords);
        cout << "Next guess should be: " << lastGuess << "\n";
      } else {
        cout << "You won!\n";
        break;
      }
    }
  } while (command != "quit");
}

namespace Testing {
// TestGeneratePattern: Verifies that GeneratePattern returns the expected
// pattern for given guess and actual word combinations.
void TestGeneratePattern() {
  struct TestCase {
    string guess;
    string actual;
    Pattern expected;
  };

  vector<TestCase> testCases = {
      // Case 1: All Gray (no letters match)
      {"CRANE", "LIGHT", 0b0000000000},  // 10 bits for 5 letters (all Gray)
      // Case 2: All Green (perfect match)
      {"CRANE", "CRANE", 0b1010101010},  // All letters Green
      // Case 3: One Green, Rest Gray
      {"CRANE", "BLAME", 0b0000100010},  // Expected pattern: Only 'E' is Green
                                         // (adjust accordingly)
      // Case 4: Some Yellows (correct letters, wrong positions)
      {"CRANE", "LEMON", 0b0000000101},  // Expected pattern for yellows
      // Case 5: Duplicates (correct handling of duplicate letters)
      {"APPLE", "PLATE",
       0b0101000110},  // Expected pattern: P & L Yellow, E Green
      // Case 6: Multiple Occurrences (handling repeated letters)
      {"MAMBO", "AMAZE", 0b0101000000}  // Expected pattern: M & A Green, B Gray
  };

  for (const auto &test : testCases) {
    Pattern result = GeneratePattern(test.guess, test.actual, 5);
    assert(result == test.expected &&
           "Test case failed in TestGeneratePattern!");
  }
  cout << "✅ All GeneratePattern tests passed!" << endl;
}

// TestCalculateEntropy: Checks that CalculateEntropy returns correct entropy
// values for known distributions of patterns.
void TestCalculateEntropy() {
  // Case 1: Single pattern → entropy should be 0.
  unordered_map<Pattern, int> case1 = {
      {242, 100}  // 22222 in base-3 (if using that encoding) → 242 in decimal
  };
  assert(CalculateEntropy(case1) == 0.0);

  // Case 2: Two equally likely patterns → entropy should be 1 bit.
  unordered_map<Pattern, int> case2 = {{242, 50}, {0, 50}};
  assert(abs(CalculateEntropy(case2) - 1.0) < 1e-6);

  // Case 3: Three unequal probabilities.
  unordered_map<Pattern, int> case3 = {{242, 70}, {0, 20}, {27, 10}};
  double entropyCase3 = CalculateEntropy(case3);
  assert(entropyCase3 > 0.0 && entropyCase3 < 1.6);

  // Case 4: Equal distribution over 4 patterns → entropy should be 2 bits.
  unordered_map<Pattern, int> case4 = {{242, 25}, {0, 25}, {27, 25}, {81, 25}};
  assert(abs(CalculateEntropy(case4) - 2.0) < 1e-6);

  cout << "✅ All CalculateEntropy tests passed!" << endl;
}

// TestPlayRandomWordleGame: Simulates 100 random games and prints the success
// and failure counts.
void TestPlayRandomWordleGame() {
  int success = 0;
  int fail = 0;
  Wordle wordle(5, "./WordNet-3.0");
  for (int i = 0; i < 100; ++i) {
    auto res = wordle.PlayRandomGame();
    if (res.has_value()) {
      success++;
    } else {
      fail++;
    }
  }
  cout << "Success: " << success << " Fail: " << fail << endl;
}
}  // namespace Testing

//////////////////////////
// Main Function        //
//////////////////////////
int main() {
  cout << "Enter the size of the word: " << endl;
  string wordSizeAsString;
  getline(cin, wordSizeAsString);
  int wordSize = stoi(wordSizeAsString);
  assert(wordSize > 2 && wordSize < 9);  // Ensure word size is between 3 and 8

  // Create a Wordle object with the given word size and dictionary path.
  Wordle wordle(wordSize, "./WordNet-3.0");

  // Uncomment these lines to run tests:
  // Testing::TestGeneratePattern();
  // Testing::TestCalculateEntropy();
  // Testing::TestPlayRandomWordleGame();

  // Start the offline interactive game.
  wordle.PlayOffLineGame();
  return 0;
}
