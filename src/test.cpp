#include "openfhe.h"
#include "utils.h"


int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Need number of seeds and number seeds input \n";
        return 1;
    }
    int seed = std::stoi(argv[1]);
    int seed_input = std::stoi(argv[2]);

    auto config = loadConfig("config.txt");

    bool encryptON       = std::stoi(config["encryptON"]);
    bool categorizeON    = std::stoi(config["categorizeON"]);
    uint32_t RNS_size   = std::stoul(config["RNS_limbs"]);
    bool withNTT         = std::stoi(config["withNTT"]);
    uint32_t firstMod    = std::stoul(config["firstMod"]);
    uint32_t scaleMod    = std::stoul(config["scaleMod"]);
    uint32_t logN = std::stoul(config["logN"]);
    uint32_t ringDim     = 1 << logN;
    uint32_t gap         = std::stoul(config["gap"]);
    int logMin           = std::stoi(config["logMin"]);
    int logMax           = std::stoi(config["logMax"]);

    std::string prelog = "logs/";
    std::string info = "log_"+std::to_string(RNS_size) + "_" + std::to_string(withNTT)+"_" + std::to_string(logN) + "_" + std::to_string(firstMod) + "_" +
                                        std::to_string(scaleMod) + "_" + std::to_string(gap) +"_" + std::to_string(logMin) + "_" + std::to_string(logMax) +"/";

    std::string dir_log = prelog + info;
    std::string endFile = "_" + std::to_string(seed) + "_" + std::to_string(seed_input) + ".txt";

    std::ofstream norm2File(dir_log+"log_norm2/out_norm2"+endFile);
    std::cout << dir_log << " asd" << std::endl;
    uint32_t multDepth = RNS_size;
    pid_t pid = getpid();
    std::string pid_str = std::to_string(pid);
    std::string seedFileStr = "seeds/seed_"+pid_str+".txt";
    std::ofstream seedFile(seedFileStr, std::ofstream::out);
            // Verifica si el archivo se abrió correctamente
    if (!seedFile.is_open()) {
        std::cerr << "No se pudo abrir el archivo de semillas en " << seedFileStr << std::endl;
        return 1;
    }
    seedFile << seed;
    seedFile.close();

    uint32_t batchSize = ringDim >> 1;
    if(gap>0)
        batchSize = batchSize >> gap;
    ScalingTechnique rescaleTech = FIXEDMANUAL;
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleMod);
    parameters.SetFirstModSize(firstMod);
    parameters.SetBatchSize(batchSize);
    parameters.SetRingDim(ringDim);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetSecurityLevel(HEStd_NotSet);
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);
    cc->Enable(PKE);
    cc->Enable(LEVELEDSHE);

    auto keys = cc->KeyGen();

    std::vector<double> input = uniform_dist(batchSize, logMin, logMax, seed_input, false);

    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    bool (lbcrypto::CKKSPackedEncoding::*a)() = &lbcrypto::CKKSPackedEncoding::Encode;
    std::cout << a << std::endl;
    auto c_original = cc->Encrypt(keys.publicKey, ptxt1);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);

    Plaintext golden_result;
    cc->Decrypt(keys.secretKey, c, &golden_result);
    golden_result->SetLength(batchSize);
    std::vector<double> golden_result_vec = golden_result->GetRealPackedValue();
    double golden_norm2 = norm2(input, golden_result_vec, batchSize);

    if (golden_norm2 < 0.1)
    {
        Plaintext result_bitFlip;
        size_t numBitsCoeffs = 64;
        size_t RNS_size = multDepth + 1;
        double norm2_abs = 0;
        std::string norms2;
        c = cc->Encrypt(keys.publicKey, ptxt1);
        c->GetElements()[k].GetAllElements()[i][j];

        cc->Decrypt(keys.secretKey, c, &result_bitFlip);
        result_bitFlip->SetLength(batchSize);
        std::vector<double> result_bitFlip_vec = result_bitFlip->GetRealPackedValue();

        norm2_abs = norm2(golden_result_vec, result_bitFlip_vec,batchSize);
        norms2.append(std::to_string(norm2_abs)+ ", ");
        if (norm2File.is_open())
            norm2File << norms2;
    }
    else
        std::cout << "ERROR!!! Norm2: " << golden_norm2 << "  Input/output: " << input << " " << golden_result  << std::endl;
    return 0;
}

