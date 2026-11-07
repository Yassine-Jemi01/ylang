# Enums and exhaustive matching (experimental)

YLang's current enum milestone supports **unit enums**: named alternatives with no attached payload. Every enum declaration creates a distinct type, even when another enum has the same variants.

```yl
enum Direction { North, East, South, West }

function is_east(Direction direction) -> bool {
    return direction == Direction.East;
}

function main() -> int {
    let Direction facing = Direction.North;
    match (facing) {
        Direction.North -> { print("north"); }
        Direction.East -> { print("east"); }
        Direction.South -> { print("south"); }
        Direction.West -> { print("west"); }
    }
    return 0;
}
```

## Rules

- Declare the enum before using its type or variants.
- Write variants as `EnumName.Variant`.
- Enum values can be stored in variables, passed to functions, returned, and compared with `==` and `!=`.
- Assigning values from different enum types is a type error.
- `match (value)` must include every variant exactly once. Missing variants and duplicate arms are errors.
- Each arm uses `Variant -> statement`; a block is recommended for clarity.
- In this milestone, match values must be simple side-effect-free expressions; store a value in a variable before matching it.
- Enums are represented as integer tags in the C backend. Do not depend on the numeric tag values.
- Payload variants, generic enums, matching bindings, and NASM parity beyond the tested unit-enum subset are not promised as stable.

The syntax and semantics are experimental until the feature PR is reviewed and merged. See [YLang 2.0 design](YLANG-2.0-DESIGN.md) for the remaining value-type roadmap.
