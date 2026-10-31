#!/usr/bin/env python3
"""Validate the public shape of ylang.mir JSON emitted by the compiler."""

import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_ir_json.py <module.json>")
    payload = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    assert payload["format"] == "ylang.mir"
    assert payload["version"] == 1
    functions = payload["functions"]
    assert isinstance(functions, list) and functions

    calls = []
    for function in functions:
        assert isinstance(function["name"], str)
        assert isinstance(function["return_type"], str)
        assert isinstance(function["parameters"], list)
        assert isinstance(function["blocks"], list) and function["blocks"]
        for parameter in function["parameters"]:
            assert isinstance(parameter["id"], int)
            assert isinstance(parameter["type"], str)
        for block in function["blocks"]:
            assert isinstance(block["id"], int)
            assert isinstance(block["parameters"], list)
            instructions = block["instructions"]
            assert isinstance(instructions, list) and instructions
            assert instructions[-1]["opcode"] in {"br", "cond_br", "return"}
            for instruction in instructions:
                assert isinstance(instruction["opcode"], str)
                assert "source" in instruction
                assert "arguments" in instruction
                assert "false_arguments" in instruction
                if instruction["opcode"] == "call":
                    calls.append(instruction)
                    assert isinstance(instruction["callee"], str)
                    assert isinstance(instruction["callee_symbol"], str)
                    assert isinstance(instruction["arguments"], list)
                    assert isinstance(instruction["call_parameter_types"], list)

    assert calls, "fixture should contain at least one direct imported function call"
    assert any(function["symbol"] for function in functions)
    print("YLang JSON IR schema test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
