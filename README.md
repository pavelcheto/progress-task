# progress-task

Interview task for Progress MOVEit

## Description

Simple file upload program

## Getting Started

### Dependencies

* Linux based OS
* libcurl (libcurl4-openssl-dev for Debian based)
* C++ compiler with C++17 support (i.e. g++8)
* cmake

### Build

```
cd progress-task
mkdir build && cd build
cmake ..
make
```

### Executing the program

```
./progress-task -u username -p password -f file_path
```

## Authors

Pavel Vasilev

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

* [nlohmann/json](https://github.com/nlohmann/json)