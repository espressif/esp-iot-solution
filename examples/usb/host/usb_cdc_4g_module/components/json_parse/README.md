# JSON Parser

This is a simple, light weight JSON parser built on top of [jsmn](https://github.com/zserge/jsmn).

Files

- `src/json_parser.c`: Source file which has all the logic for implementing the APIs built on top of JSMN
- `include/json_parser.h`: Header file that exposes all APIs
- `test/main.c`: A test file which demonstrates parsing of a pre-defined JSON
- `Makefile`: For generating the test executable

# Usage

Clone the repository using: `git clone --recursive https://github.com/shahpiyushv/json_parser.git`

> Note: The --recursive argument is important because json\_parser has jsmn as a git submodule,
which will get cloned with the --recursive argument.
> If you forget it, just execute `git submodule update --init --recursive` from json\_parser/.

Include the `src/json_parser.c` and `include/json_parser.h` files in your project's build system and that should be enough.
`json_parser` requires only standard library functions and jsmn for compilation.

# Testing
- To compile the test executable, just execute `make`.
- This will create `json_parser` binary.
- Running the binary should print the parsed information

```text
./json_parser
str_val JSON Parser
float_val 2.000000
int_val 2017
bool_val false
Array has 6 elements
index 0: bool
index 1: int
index 2: float
index 3: str
index 4: object
index 5: array
Found object
objects true
arrays yes
int64_val 109174583252
```

To cleanup the app, execute `make clean`
