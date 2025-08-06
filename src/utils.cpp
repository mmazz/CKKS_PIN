#include "utils.h"
const std::string DATAFOLDER = "/home/mmazz/CKKS_PIN/counterData";
std::string ccLocation       = "/cryptocontext.txt";
std::string pubKeyLocation   = "/key_pub.txt";
std::string secKeyLocation   = "/key_sec.txt";
std::string input_vec = "/input.txt";


std::vector<double> load_vector(const std::string& filename) {
    std::vector<double> vec;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo para lectura: " << filename << std::endl;
        return vec;
    }

    double val;
    while (file >> val) {
        vec.push_back(val);
    }

    file.close();
    return vec;
}
void save_vector(const std::vector<double>& vec, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al abrir el archivo para escritura: " << filename << std::endl;
        return;
    }

    for (double val : vec) {
        file << val << '\n';  // escribe cada valor en una línea nueva
    }

    file.close();
}


void testVoid(){
}
double norm2(std::vector<double>  &vecInput, std::vector<double> &vecOutput, size_t size){
    double res = 0;
    double diff = 0;
    // Itero sobre el del input por si el del output por construccion quedo mas grande
    for (size_t i=0; i<size; i++)
    {
        diff = vecOutput[i] - vecInput[i];
        res += pow(diff, 2);
    }
    res = std::sqrt(res/size);
    return res;
}

std::unordered_map<std::string, std::string> loadConfig(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo de configuración: " << filename << "\n";
        return config;  // vacío
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;

        // Ignorar líneas vacías y comentarios
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            config[key] = value;
        } else {
            std::cerr << "Advertencia: línea inválida en config.txt (línea " << lineNumber << "): " << line << "\n";
        }
    }

    return config;
}
std::vector<double> uniform_dist(uint32_t batchSize, uint64_t  logMin, uint64_t logMax, int seed, bool verbose){

    std::vector<double> input(batchSize, 0.0);

    if (logMin >= logMax) {
        throw std::invalid_argument("Se requiere que minBits < maxBits.");
    }
    if (verbose)
        std::cout << "Parameters: batchSize=" << batchSize<< ", minBits="<< logMin << ", maxBits=" << logMax << ", seed=" << seed <<std::endl;


    std::random_device rd;
    std::mt19937_64 gen(seed);

    double max_val = std::pow(2.0, logMax);
    double min_val = std::pow(2.0, logMin);
    if (min_val==1)
        min_val = 0;
    if (max_val==1)
        max_val = 0;

    if (verbose)
        std::cout << "Range = [" << min_val << ", " << max_val << "]"<<std::endl;

    std::uniform_real_distribution<double> dist(min_val, max_val);

    for (uint32_t i = 0; i < batchSize; ++i) {
        input[i] = dist(gen);
        if (verbose)
            std::cout << input[i] << ", ";
    }

    return input;
}

