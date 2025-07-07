# Wordle Solver

This project contains a command line Wordle solver written in modern C++.
The solver loads a dictionary derived from the WordNet 3.0 word lists and
uses an entropy based strategy to suggest guesses.

## Features

- Support for word lengths between 3 and 8 characters
- Interactive offline mode with step by step suggestions
- Ability to play random games for automated testing
- Dictionary files from WordNet 3.0 included in the repository

## Requirements

- A C++20 compatible compiler
- CMake **3.30** or later

## Building

Use CMake to configure and build the executable:

```bash
cmake -S . -B build
cmake --build build
```

The resulting executable will be located at `build/wordle`.

## Running

From the project root run the compiled executable:

```bash
./build/wordle
```

When launched you are prompted to enter the word size. After that the
program enters an interactive session where you can request guesses and
filter the candidate list based on your feedback.

### Offline mode commands

- `word`    – manually enter your chosen starting word
- `guess`   – ask the solver for the next guess
- `remove`  – remove the last guess from the dictionary
- `quit`    – exit the program

After each guess you will be asked for the result pattern for each
letter using the following codes:

- `gn` – the letter is correct and in the correct position (green)
- `y`  – the letter is present but in a different position (yellow)
- `gr` – the letter is not present in the word (gray)

The solver updates its internal list of candidates according to the
pattern you provide and displays the next recommended guess.

## WordNet dictionary

The `WordNet-3.0` folder contains the index files from the official
WordNet 3.0 distribution. These files are used to build the dictionary
of possible words. WordNet is distributed by Princeton University under
a permissive license; see `WordNet-3.0/LICENSE` for details.

## Tests

A few unit tests exist in `wordle.cpp` within the `Testing` namespace.
They are commented out by default. You can enable them by uncommenting
the lines in `main()` and rebuilding the project.

## License

This repository includes material from WordNet 3.0 which is licensed by
Princeton University. See the license file in the `WordNet-3.0`
subdirectory for the full terms.
