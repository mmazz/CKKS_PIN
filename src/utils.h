#ifndef UTILS_MATI_H
#define UTILS_MATI_H

#include "openfhe.h"

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <unordered_map>
#include <iostream>
#include <iostream>
#include <sstream>
using namespace lbcrypto;

using Integer  = intnat::NativeIntegerT<long unsigned int>;


void testVoid();
std::unordered_map<std::string, std::string> loadConfig(const std::string& filename);
std::vector<double> uniform_dist(uint32_t batchSize, uint64_t  logMin, uint64_t logMax, int seed, bool verbose=false);

double norm2(std::vector<double>  &vecInput, std::vector<double> &vecOutput, size_t size);
#endif

