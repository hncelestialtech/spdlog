# Cel Spdlog

## Usage

Make sure to install conan and cmake before building.

Install the conan package:
```bash
make install
```

Use the package in your project:
```
# conanfile.txt
requires = "cel_spdlog/1.12.0"
```

```cmake
find_package(cel_spdlog REQUIRED)

target_link_libraries(my_target PRIVATE cel_spdlog::cel_spdlog)
```

In the source code, use the logger like this:
```cpp
int log_cpu_id = 0;
std::string log_dir = "./logs";
std::string log_name = "my_logger";
std::string log_level = "info"; // options: "debug", "info", "warn", "error"

std::shared_ptr<spdlog::logger> logger = Cel::Log::create_logger(log_cpu_id, log_name, log_dir, log_level);
```

