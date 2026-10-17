#include "azazel6/module.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        auto rt = std::make_unique<psprecomp::Runtime>();
        azazel6::register_module_hle(*rt, 0x08804000u, 0x2000u);
        auto& c = rt->cpu();
        for (auto address : {0x08804000u, 0x08804001u, 0x08805FFFu, 0x08803FFFu,
                             0x08806000u, 0u, 0x48804000u, 0x88804000u, 0xFFFFFFFFu}) {
            c.pc = 0x08805000u; c.gpr[31] = 0x08805100u; c.gpr[4] = address;
            azazel6::invoke_returning_import(*rt, c, "ModuleMgrForUser", 0xD8B73127u);
            const auto expected = address & 0x80000000u ? 0x800200D3u :
                address >= 0x08804000u && address < 0x08806000u ? azazel6::kMainModuleUid : 0x8002012Eu;
            check(c.gpr[2] == expected && c.pc == c.gpr[31] && !rt->stopped(), "Module identity/return boundary");
        }
        for (auto [begin, size] : {std::pair{0x08804000u, 0u}, {0xFFFFFFFFu, 2u},
                                  {0x08000000u, 0xFFFFFFFFu}, {0x07FFFFFFu, 1u}}) {
            bool rejected = false;
            try { azazel6::register_module_hle(*rt, begin, size); }
            catch (const std::exception&) { rejected = true; }
            check(rejected, "Invalid image accepted");
        }
        std::cout << "Resident module identity contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
