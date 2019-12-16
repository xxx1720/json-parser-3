#include "jsonparser.h"
#include <iostream>
#include <memory>


using namespace std;
using namespace jsonparser;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << argv[0] << " <filename> [-f]";
    return 0;
  }

  json_parser ps(argv[1]);
  while (ps.step())
    ;

  if (argc >= 3 && !strcmp(argv[2], "-f"))
    ps.entry()->print(std::cout, true);
  else
    ps.entry()->print(std::cout);

  return 0;
}