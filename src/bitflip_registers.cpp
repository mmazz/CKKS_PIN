#include "openfhe.h"
#include "utils.h"

int main(int argc, char* argv[]) {
    const char* home = getenv("HOME");
    uint32_t firstMod    = 60;
    uint32_t scaleMod    = 50;
    uint32_t logN = 3;
    uint32_t ringDim     = 1 << logN;

    uint32_t multDepth = 0;

    uint32_t batchSize = ringDim >> 1;

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

    std::vector<double> input = uniform_dist(batchSize, 0, 8, 0, false);

    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(input);
    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
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
        cc->Decrypt(keys.secretKey, c, &result_bitFlip);
        result_bitFlip->SetLength(batchSize);
        std::vector<double> result_bitFlip_vec = result_bitFlip->GetRealPackedValue();
        norm2_abs = norm2(golden_result_vec, result_bitFlip_vec,batchSize);
        std::cout << "Norm2: " << norm2_abs << std::endl;

    }
    else
        std::cout << "ERROR!!! Norm2: " << golden_norm2 << "  Input/output: " << input << " " << golden_result  << std::endl;
    return 0;
}

