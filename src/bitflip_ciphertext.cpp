#include "openfhe.h"
#include "utils.h"

#define SAVEADDRES_LABEL(name) \
    asm volatile( \
        ".global " #name "\n\t" \
        ".type " #name ", @function\n\t" \
        #name ":\n\t" \
        "nop\n\t" \
        ::: "memory" \
    );

#define CHECKPOINT_LABEL(name) \
    asm volatile( \
        ".global " #name "\n\t" \
        ".type " #name ", @function\n\t" \
        #name ":\n\t" \
        "nop\n\t" \
        ::: "memory" \
    );
void TestFunction(){

}
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Need number of seeds and number seeds input \n";
        return 1;
    }
  //  int seed = std::stoi(argv[1]);
    int coeff = std::stoi(argv[1]);
    int seed_input = std::stoi(argv[2]);
    const char* home = getenv("HOME");
    auto config = loadConfig(std::string(home)+"/CKKS_PIN/config.txt");

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
  //  std::string endFile = "_" + std::to_string(seed) + "_" + std::to_string(seed_input) + ".txt";

  //  std::ofstream norm2File(dir_log+"log_norm2/out_norm2"+endFile);
    uint32_t multDepth = RNS_size;

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
        double norm2_abs = 0;
        std::string norms2;
        c = cc->Encrypt(keys.publicKey, ptxt1);
        auto raw_ctxt = c.get();

        auto& c_ptr = raw_ctxt->GetElements()[0].GetAllElements()[0][0];
        auto c_val = c->GetElements()[0].GetAllElements()[0][coeff];
        std::ofstream addr_file(std::string(home)+"/CKKS_PIN/pintools/bitflips/target_address.txt");
        addr_file << std::hex << reinterpret_cast<uintptr_t>(&c_ptr);
        addr_file.close();


        std::cout << "Address of target: 0x" << std::hex << &c_ptr << std::dec << std::endl;

SAVEADDRES_LABEL(addr_file)
CHECKPOINT_LABEL(start_checkpoint)
        TestFunction();
        cc->Decrypt(keys.secretKey, c, &result_bitFlip);
        result_bitFlip->SetLength(batchSize);
        std::vector<double> result_bitFlip_vec = result_bitFlip->GetRealPackedValue();

        norm2_abs = norm2(golden_result_vec, result_bitFlip_vec,batchSize);
        norms2.append(std::to_string(norm2_abs)+ ", ");
        std::cout << "Norm2: " << norm2_abs << std::endl;
   //     if (norm2File.is_open())
   //         norm2File << norms2;
CHECKPOINT_LABEL(end_checkpoint)
        std::cout << "Variable test was "<< c_val  <<", now: " << c->GetElements()[0].GetAllElements()[0][coeff]<< std::endl;
    }
    else
        std::cout << "ERROR!!! Norm2: " << golden_norm2 << "  Input/output: " << input << " " << golden_result  << std::endl;
    return 0;
}

