# Your First Program

Create a file named `hello.yl`:

```ylang
function main() -> int {
    let const string name = "YLang";
    print("Hello from", name);
    print("The answer is", 40 + 2);
    return 0;
}
```

The program defines the `main` function, creates a constant string, prints two lines, and returns an integer exit status.

## Check, build, and run

From the repository root:

```sh
./build/ylang check hello.yl
./build/ylang build hello.yl -o hello
./hello
```

The first command checks the source without producing an executable. The second uses the default C backend and invokes a C compiler to build the native program. The final command runs it.

On Windows, the compiler executable is normally `build/ylang.exe`; the output program should use a Windows executable name such as `hello.exe`.

## What if it fails?

Read the complete diagnostic, including its error code. Check punctuation, declared types, and the compiler command before changing the program. The [Compiler Error Codes](reference/error-codes.md) page explains stable diagnostic identifiers.

## Try one change

Change the value of `name`, or replace `40 + 2` with a different integer expression. Run the same check/build/run sequence again. The point is to build a habit: check the source first, then compile it, then run the output.
