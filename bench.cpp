#include "src/actions.hpp"
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define OP_COUNT 1000000
#define SIZE 1000

std::string valueOrNull(const Value* v) {
  if (v == nullptr)
    return "null";
  return std::to_string(*v);
}

int main() {
  int fd = open("words.txt", O_RDONLY);
  if (fd == -1) {
    perror("open");
    exit(1);
  }
  struct stat sb;
  fstat(fd, &sb);
  if (sb.st_size == 0) {
    close(fd);
    return 0;
  }
  char* addr = (char*)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    close(fd);
    exit(1);
  }

  // int perf_ctl_fd = atoi(getenv("PERF_CTL_FD"));
  // write(perf_ctl_fd, "enable\n", 8);

  const auto start_bench = std::chrono::steady_clock::now();
  Nodes::Header* root = Nodes::makeNewRoot();

  char *start = addr, *end;
  size_t value = 0;
  while ((end = strchrnul(start, '\n')) < addr + sb.st_size) {
    Actions::insert(root, (uint8_t*)start, end - start, value++);

    if (*end == '\n') {
      start = end + 1;
    } else {
      break;
    }
  }

  const auto end_bench = std::chrono::steady_clock::now();
  const auto bench_duration =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end_bench -
                                                           start_bench)
          .count();

  std::cout << "bench took " << bench_duration << "ns ("
            << bench_duration / OP_COUNT << "ns/op)" << std::endl;

  munmap(addr, sb.st_size);
  close(fd);

  Actions::insert(root, (const uint8_t*)"Aaron", 5, 10);
  std::cout << valueOrNull(Actions::search(root, (const uint8_t*)"Aaron", 5))
            << std::endl;
  std::cout << valueOrNull(Actions::search(root, (const uint8_t*)"Zyrenian", 8))
            << std::endl;
}
