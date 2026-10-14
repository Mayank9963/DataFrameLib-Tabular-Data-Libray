# DataFrameLib - COP290 Assignment 4

## Description
DataFrameLib is a high-performance C++ library for data manipulation and analysis, built on top of Apache Arrow and Parquet. It provides a robust API for handling tabular data with support for both eager and lazy execution modes. The library includes a query optimizer that performs predicate and projection pushdown, constant folding, and expression simplification to ensure efficient data processing.

## Build and Installation Instructions

Follow these steps to install the necessary dependencies and build the library on your system.

### 1. Installation of Dependencies (Prerequisites)
Before building the project, you must install the Apache Arrow and Parquet C++ libraries.

- **macOS (using Homebrew)**:
  ```bash
  brew install apache-arrow
  ```
- **Linux (Ubuntu/Debian)**:
  ```bash
  sudo apt update
  sudo apt install libarrow-dev libparquet-dev
  ```
- **Compiler**: Ensure you have a C++17 compliant compiler (GCC 9+, Clang 10+, or MSVC 2019+) and CMake (3.15+) installed.

### 2. Building the Library
To compile the source code into the `dataframelib` static library:
1.  **Navigate to the project root** and create a build directory:
    ```bash
    mkdir build && cd build
    ```
2.  **Configure and Compile**:
    ```bash
    cmake ..
    make
    ```
This will produce the `libdataframelib.a` library file in the `build` directory.

---

## Testing Methods

### 1. Manual Build and Testing
**Note**: The `tests/` folder is not included in the submitted source code. If you wish to run custom tests, follow these steps:

1.  **Create a tests directory**: `mkdir tests`
2.  **Add your test code**: Create `tests/main.cpp` and paste your test code.
3.  **Update `CMakeLists.txt`**: Append these lines to the end of your `CMakeLists.txt`:
    ```cmake
    add_executable(test_main tests/main.cpp)
    target_link_libraries(test_main PRIVATE dataframelib)
    ```
4.  **Rebuild and Run**:
    ```bash
    cd build
    cmake ..
    make
    ./test_main
    ```

### 2. Running via Autograder
The submitted code is ready for the autograder. No changes are required.

1.  **Navigate to autograder**: `cd A4-Tester-2`
2.  **Install requirements**: `pip install -r requirements.txt`
3.  **Run**: `python autograder.py --student-dir /path/to/project`

## Project Structure
- `include/`: Public headers.
- `src/`: Implementation files.
- `CMakeLists.txt`: Build configuration.
- `report.pdf`: Detailed project report.
- `README.md`: This file.
