#include "openfhe.h"
#include "utils.h"


extern "C" void addr_label();
// Aquí defines el símbolo vacío que PIN instrumentará.
asm(
    ".global addr_label       \n"
    ".type   addr_label, @function \n"
    "addr_label:              \n"
    "    nop                 \n"
    "    ret                 \n"
);

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
    auto c_original = cc->Encrypt(keys.publicKey, ptxt1);
    lbcrypto::PseudoRandomNumberGenerator::SetPRNGSeed(0);
    auto c = cc->Encrypt(keys.publicKey, ptxt1);


    c->GetElements()[0].SwitchFormat();
    auto raw_ctxt = c.get();
    auto& before_elem = raw_ctxt->GetElements()[0];
    std::cout << "Dirección antes de NTT: 0x"
              << std::hex << std::setw(sizeof(uintptr_t)*2)
              << std::setfill('0')
              << reinterpret_cast<uintptr_t>(&before_elem)
              << std::dec << "\n";

        std::cout << "Before bitflip: " << c->GetElements()[0].GetAllElements()[0][0] << std::endl;

    c->GetElements()[0].SwitchFormat();
    raw_ctxt = c.get();
    auto& after_elem = raw_ctxt->GetElements()[0]
                                     ;
    std::cout << "Dirección antes de NTT: 0x"
              << std::hex << std::setw(sizeof(uintptr_t)*2)
              << std::setfill('0')
              << reinterpret_cast<uintptr_t>(&after_elem)
              << std::dec << "\n";


    int count =0;
    auto c_vals = c->GetElements()[0].GetAllElements();
    auto c_original_vals = c_original->GetElements()[0].GetAllElements();
    for (int i=0; i<ringDim; ++i){
        if(c_vals[0][i]!= c_original_vals[0][i])
            ++count;
    }
    std::cout << "Initial Differences == " << count << std::endl;
    Plaintext golden_result;
    cc->Decrypt(keys.secretKey, c, &golden_result);
    golden_result->SetLength(batchSize);
    std::vector<double> golden_result_vec = golden_result->GetRealPackedValue();
    double golden_norm2 = norm2(input, golden_result_vec, batchSize);

    if (golden_norm2 < 0.1 && count==0)
    {
        Plaintext result_bitFlip;
        double norm2_abs = 0;
        std::string norms2;
        auto raw_ctxt = c.get();
        auto& c_elem_ptr = raw_ctxt->GetElements()[0].GetAllElements()[0][0];

        std::ofstream ofs(std::string(home) + "/CKKS_PIN/pintools/bitflips/target_address.txt");
        ofs << std::hex << reinterpret_cast<uintptr_t>(&(raw_ctxt->GetElements()[0]))<< "\n";
        ofs << std::hex << reinterpret_cast<uintptr_t>(&c_elem_ptr)<< "\n";
        ofs.close();
        addr_label();
        std::cout << "A" << std::dec << std::endl;
        testVoid();

        std::cout << "After bitflip: " << c->GetElements()[0].GetAllElements()[0][0] << std::endl;
        std::cout << "B" << std::dec << std::endl;
        cc->Decrypt(keys.secretKey, c, &result_bitFlip);
        result_bitFlip->SetLength(batchSize);
        std::vector<double> result_bitFlip_vec = result_bitFlip->GetRealPackedValue();

        for (int i=0; i<batchSize; ++i){
            std::cout <<  golden_result_vec[i] << " ? " << result_bitFlip_vec[i] << std::endl;
        }
        norm2_abs = norm2(golden_result_vec, result_bitFlip_vec,batchSize);
        std::cout << "Norm2: " << norm2_abs << std::endl;
        std::cout << "Variable test was "<< c_original->GetElements()[0].GetAllElements()[0][0]  <<", now: " << c->GetElements()[0].GetAllElements()[0][0]<< std::endl;
        count = 0;
        c_vals = c->GetElements()[0].GetAllElements();
        c_original_vals = c_original->GetElements()[0].GetAllElements();
        for (int i=0; i<ringDim; ++i){
            if(c_vals[0][i]!= c_original_vals[0][i])
                ++count;
        }
        std::cout << "Differences == " << count << std::endl;
    }
    else
        std::cout << "ERROR!!! Norm2: " << golden_norm2 << "  Input/output: " << input << " " << golden_result  << std::endl;
    return 0;
}

