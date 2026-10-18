# Guarded formatter jump table

**VERIFIED, 2026-10-18:** the formatter at `0x08B7E7B4` bounds its character-derived
index below 89, loads a word from the table at `0x08BD93EC` and jumps through a0
at `0x08B7E924`. Profile metadata records that site, register, table start and count.
Generation validates the JR encoding, the complete file-backed table and each
aligned executable target. The CFG follows those destinations; it does not scan
the table as code. Branches into delay slots remain rejected.

A generated runtime guard at the JR label checks the dynamic destination against
the validated table, before the delay slot or dispatch. The same guard machinery
also protects the constructor JALR. Duplicate/missing labels and unused maps fail
generation. The framework checkout remains unchanged.

Twenty roots now contain 2878 instruction words. The native probe enters the
formatter, then stops at uncovered helper `0x08B83138`, return `0x08B7E7F4`.
This proves entry into the formatter; it does not claim a completed formatting
operation or that every case in the jump table has executed.

34 Python tests and all nine CTest suites passed. Two upstream CTest suites first
failed sandbox temporary-file operations, then passed outside the sandbox.
The generated probe compiled with MSVC and Android NDK ARM64. Execution retained
4096 dispatches, a 15-second external timeout and disabled interpreter fallback.
