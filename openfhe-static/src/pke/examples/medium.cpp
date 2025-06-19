#define PROFILE

#include "openfhe.h"

using namespace lbcrypto;

int main() {


    uint32_t multDepth = 0;
    uint32_t scaleModSize = 15;
    uint32_t firstModSize = 20;
    uint32_t ringDim = 16;
    uint32_t batchSize = ringDim >> 1;

    ScalingTechnique rescaleTech = FIXEDMANUAL;
    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetFirstModSize(firstModSize);
    parameters.SetBatchSize(batchSize);
    parameters.SetRingDim(ringDim);
    parameters.SetScalingTechnique(rescaleTech);
    parameters.SetSecurityLevel(HEStd_NotSet);
    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // Enable the features that you wish to use
    cc->Enable(PKE);
    cc->Enable(LEVELEDSHE);
    std::cout << "CKKS scheme is using ring dimension " << cc->GetRingDimension() << std::endl << std::endl;

    auto keys = cc->KeyGen();

    std::vector<double> x1 = {0.25, 1.1, -1.2, 3};

    Plaintext ptxt1 = cc->MakeCKKSPackedPlaintext(x1);

    std::cout << "Input x1: " << ptxt1 << std::endl;

    // Encrypt the encoded vectors
    auto c1 = cc->Encrypt(keys.publicKey, ptxt1);

    // Step 5: Decryption and output
    Plaintext result;

    std::cout.precision(8);

    std::cout << std::endl << "Results of homomorphic computations: " << std::endl;

    cc->Decrypt(keys.secretKey, c1, &result);
    result->SetLength(batchSize);
    std::cout << "x1 = " << result;
    std::cout << "Estimated precision in bits: " << result->GetLogPrecision() << std::endl;

    return 0;
}
