#ifndef UTILS_MATI_H
#define UTILS_MATI_H

#include "openfhe.h"

#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <sstream>
using namespace lbcrypto;

using Integer  = intnat::NativeIntegerT<long unsigned int>;
extern const std::string DATAFOLDER ;
extern std::string ccLocation       ;
extern std::string pubKeyLocation   ;
extern std::string secKeyLocation   ;
extern std::string input_vec ;

std::vector<double> load_vector(const std::string& filename);
void save_vector(const std::vector<double>& vec, const std::string& filename);



void testVoid();
std::unordered_map<std::string, std::string> loadConfig(const std::string& filename);
std::vector<double> uniform_dist(uint32_t batchSize, uint64_t  logMin, uint64_t logMax, int seed, bool verbose=false);

double norm2(std::vector<double>  &vecInput, std::vector<double> &vecOutput, size_t size);
#endif

