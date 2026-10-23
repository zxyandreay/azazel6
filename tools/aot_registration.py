"""Bound host stack use in the pinned generator's registration function."""
import re


def compact_registration(source):
    """Preserve registration order/values; reject unexpected generator output.

    MSVC /Od can allocate a distinct string temporary for every statement in
    the original function. A static table and one call site keep stack use
    independent of the number of registered guest instruction addresses.
    """
    signature = "void register_generated_functions(Runtime &runtime) {\n"
    if source.count(signature) != 1:
        raise ValueError("Expected one generated registration function")
    begin = source.index(signature) + len(signature)
    end = source.find("\n}", begin)
    if end < 0:
        raise ValueError("Unterminated generated registration function")
    rows = []
    addresses = set()
    pattern = re.compile(
        r'    runtime\.register_function\((0x[0-9A-F]{8}u), '
        r'&(entry_range_[0-9]+), "(entry_range_[0-9]+)"\);')
    for line in source[begin:end].splitlines():
        match = pattern.fullmatch(line)
        if not match:
            raise ValueError("Unexpected generated registration statement")
        address, function, name = match.groups()
        if function != name or address in addresses or int(address[2:-1], 16) % 4:
            raise ValueError("Invalid or duplicate generated registration")
        addresses.add(address)
        rows.append(f'        {{{address}, &{function}, "{name}"}},')
    if not rows:
        raise ValueError("Empty generated registration function")
    body = (
        "    struct Registration {\n"
        "        std::uint32_t address;\n"
        "        Runtime::RecompiledFunction function;\n"
        "        const char* name;\n"
        "    };\n"
        "    static const Registration registrations[] = {\n"
        + "\n".join(rows) + "\n"
        "    };\n"
        "    for (const auto& entry : registrations)\n"
        "        runtime.register_function(entry.address, entry.function, entry.name);")
    return source[:begin] + body + source[end:]
